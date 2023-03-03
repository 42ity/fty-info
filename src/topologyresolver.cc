/*  =========================================================================
    topologyresolver - Class for asset location recursive resolving

    Copyright (C) 2014 - 2020 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

/*
@header
    topologyresolver - Class for asset location recursive resolving
@discuss
@end
*/

#include "topologyresolver.h"
#include <malamute.h>
#include <map>
#include <set>
#include <string>
#include <fty_log.h>
#include <fty_common.h>
#include "fty_info.h"

// State
#define DEFAULT_ENDPOINT "ipc://@/malamute"

typedef enum
{
    DISCOVERING = 0,
    UPTODATE
} ResolverState;

//  Structure of our class

struct _topologyresolver_t
{
    char*         iname;
    char*         topology;
    char*         endpoint;
    ResolverState state;
    zhashx_t*     assets;
    mlm_client_t* client;
};

static std::map<std::string, std::set<std::string>> s_local_addresses()
{
    std::map<std::string, std::set<std::string>> result;

    struct ifaddrs *interfaces = nullptr;
    if (getifaddrs(&interfaces) == -1) {
        return result;
    }

    char host[NI_MAXHOST];

    for (struct ifaddrs *iface = interfaces; iface; iface = iface->ifa_next) {
        if (!iface->ifa_addr)
            continue;
        memset(host, 0, sizeof(host));
        int family = iface->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6) {
            if (getnameinfo(iface->ifa_addr,
                    (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6), host, NI_MAXHOST,
                    nullptr, 0, NI_NUMERICHOST) == 0) {
                // sometimes IPv6 addres looks like ::2342%IfaceName
                char* p = strchr(host, '%');
                if (p)
                    *p = 0;

                auto it = result.find(iface->ifa_name);
                if (it == result.end()) {
                    std::set<std::string> aSet;
                    aSet.insert(host);
                    result[iface->ifa_name] = aSet;
                } else {
                    result[iface->ifa_name].insert(host);
                }
            }
        }
    }
    freeifaddrs(interfaces);
    return result;
}

// check if this is our rack controller - is any IP address
// of this asset the same as one of the local addresses?
static bool s_is_this_me(fty_proto_t* asset)
{
    bool itsMe = false;

    const char* operation = fty_proto_operation(asset);

    if (streq(operation, FTY_PROTO_ASSET_OP_CREATE)
        || streq(operation, FTY_PROTO_ASSET_OP_UPDATE))
    {
        const char* type    = fty_proto_aux_string(asset, "type", "");
        const char* subtype = fty_proto_aux_string(asset, "subtype", "");

        // are we creating/updating a rack controller?
        if (streq(type, "device") && streq(subtype, "rackcontroller")) {
            auto     ifaces = s_local_addresses();
            zhash_t* ext    = fty_proto_ext(asset);
            char ipKey[16];

            int index = 1;
            while (!itsMe) { // explore ipv6
                snprintf(ipKey, sizeof(ipKey), "ipv6.%d", index);
                char* ip = reinterpret_cast<char*>(zhash_lookup(ext, ipKey));
                if (!ip) break;
                index++;

                for (const auto& iface : ifaces) {
                    if (iface.second.find(ip) != iface.second.end()) {
                        itsMe = true;
                        break;
                    }
                }
            }

            index = 1;
            while (!itsMe) { // explore ipv4
                snprintf(ipKey, sizeof(ipKey), "ip.%d", index);
                char* ip = reinterpret_cast<char*>(zhash_lookup(ext, ipKey));
                if (!ip) break;
                index++;

                for (const auto& iface : ifaces) {
                    if (iface.second.find(ip) != iface.second.end()) {
                        itsMe = true;
                        break;
                    }
                }
            }
        }
    }

    return itsMe;
}

static void s_purge_message_cache(topologyresolver_t* self)
{
    if (!self || !self->assets)
        return;

    zlistx_t* topo   = topologyresolver_to_list(self);
    zlistx_t* inames = zhashx_keys(self->assets);

    char* iname = static_cast<char*>(zlistx_first(inames));
    while (iname) {
        if (!zlistx_find(topo, iname) && !streq(self->iname, iname)) {
            // asset is not me neither parent
            zhashx_delete(self->assets, iname);
        }
        iname = static_cast<char*>(zlistx_next(inames));
    }
    zlistx_destroy(&topo);
    zlistx_destroy(&inames);
}

//  --------------------------------------------------------------------------
//  Create a new topologyresolver

topologyresolver_t* topologyresolver_new(const char* iname)
{
    topologyresolver_t* self = static_cast<topologyresolver_t*>(zmalloc(sizeof(topologyresolver_t)));
    if (!self) {
        log_error("topologyresolver_t allocation failed");
        return nullptr;
    }

    memset(self, 0, sizeof(*self));

    //  Initialize class properties here
    self->iname = iname ? strdup(iname) : NULL;
    self->state  = DISCOVERING;
    self->client = mlm_client_new();

    self->assets = zhashx_new(); // map <iname, fty_proto_t*>
    zhashx_set_destructor(self->assets, reinterpret_cast<czmq_destructor*>(fty_proto_destroy));
    zhashx_set_duplicator(self->assets, reinterpret_cast<czmq_duplicator*>(fty_proto_dup));

    return self;
}

//  --------------------------------------------------------------------------
//  Destroy the topologyresolver

void topologyresolver_destroy(topologyresolver_t** self_p)
{
    if (self_p && *self_p) {
        topologyresolver_t* self = *self_p;

        //  Free class properties here
        zstr_free(&self->iname);
        zstr_free(&self->topology);
        zstr_free(&self->endpoint);
        zhashx_destroy(&self->assets);
        mlm_client_destroy(&self->client);

        //  Free object itself
        free(self);
        *self_p = nullptr;
    }
}

//  --------------------------------------------------------------------------
//  set endpoint of topologyresolver
void topologyresolver_set_endpoint(topologyresolver_t* self, const char* endpoint)
{
    if (!(self && endpoint))
        return;

    zstr_free(&self->endpoint);
    self->endpoint = strdup(endpoint);
    mlm_client_connect(self->client, self->endpoint, 1000, "fty_info_topologyresolver");
}

//  --------------------------------------------------------------------------
//  get RC internal name
//  returned char* must be freed

char* topologyresolver_id(topologyresolver_t* self)
{
    return (self && self->iname) ? strdup(self->iname) : nullptr;
}

//  --------------------------------------------------------------------------
//  Give topology resolver one asset information
bool topologyresolver_asset(topologyresolver_t* self, fty_proto_t* message)
{
    if (!self || !message)
        return false;

    if (fty_proto_id(message) != FTY_PROTO_ASSET)
        return false;

    const char* operation = fty_proto_operation(message);
    // discard inventory due to the lack of some field.
    if (operation && streq(operation, "inventory")) {
        return false;
    }

    const char* iname = fty_proto_name(message);
    // is this message about me?
    if (!self->iname && s_is_this_me(message)) {
        self->iname = strdup(iname);
        // previous code wasn't doing republish at this point
        return false;
    }

    if (self->iname && streq(self->iname, iname)) {
        // we received a message about ourselves, trigger recomputation
        zhashx_update(self->assets, iname, message);
        zlistx_t* list = topologyresolver_to_list(self);
        if (zlistx_size(list) == 0) {
            // Can't resolve topology any more
            self->state = DISCOVERING;
            zlistx_destroy(&list);
            return false;
        }
        zlistx_destroy(&list);
        return true;
    }

    // is this message about my parent?
    if (self->state == DISCOVERING) {
        // discovering - every asset (except me) is a possible parent
        zhashx_update(self->assets, iname, message);
        zlistx_t* list = topologyresolver_to_list(self);
        if (zlistx_size(list) != 0) {
            self->state = UPTODATE;
            s_purge_message_cache(self);
            zlistx_destroy(&list);
            return true;
        }
        zlistx_destroy(&list);
        return false;
    }

    if (self->state == UPTODATE) {
        // up to date - check assets in cache
        fty_proto_t* iname_msg = static_cast<fty_proto_t*>(zhashx_lookup(self->assets, iname));
        if (iname_msg) {
            // we received a message about asset in our topology, trigger recomputation
            zhashx_update(self->assets, iname, message);
            zlistx_t* list = topologyresolver_to_list(self);
            if (zlistx_size(list) == 0) {
                // Can't resolve topology any more
                self->state = DISCOVERING;
                zlistx_destroy(&list);
                return false;
            }
            zlistx_destroy(&list);
            return true;
        }
        return false;
    }

    log_debug("case not handled (state: %d)", self->state);

    return false;
}

//  --------------------------------------------------------------------------
// Return URI of asset for this topologyresolver
char* topologyresolver_to_rc_name_uri(topologyresolver_t* self)
{
    if (self && self->iname) {
        return zsys_sprintf("/asset/%s", self->iname);
    }
    return nullptr;
}

//  --------------------------------------------------------------------------
//  Return URI of the asset's parent
char* topologyresolver_to_parent_uri(topologyresolver_t* self)
{
    if (self && self->iname) {
        fty_proto_t* rc_message = static_cast<fty_proto_t*>(zhashx_lookup(self->assets, self->iname));
        if (rc_message) {
            const char* parent_iname = fty_proto_aux_string(rc_message, "parent_name.1", nullptr);
            if (parent_iname) {
                return zsys_sprintf("/asset/%s", parent_iname);
            }
        }
    }
    return nullptr;
}

//  --------------------------------------------------------------------------
//  Return user-friendly name of the asset
char* topologyresolver_to_rc_name(topologyresolver_t* self)
{
    if (self && self->iname) {
        fty_proto_t* rc_message = static_cast<fty_proto_t*>(zhashx_lookup(self->assets, self->iname));
        if (rc_message) {
            const char* name = fty_proto_ext_string(rc_message, "name", nullptr);
            if (name) {
                return strdup(name);
            }
        }
    }
    return nullptr;
}

//  --------------------------------------------------------------------------
//  Return description of the asset
char* topologyresolver_to_description(topologyresolver_t* self)
{
    if (self && self->iname) {
        fty_proto_t* rc_message = static_cast<fty_proto_t*>(zhashx_lookup(self->assets, self->iname));
        if (rc_message) {
            const char* description = fty_proto_ext_string(rc_message, "description", nullptr);
            if (description) {
                return strdup(description);
            }
        }
    }
    return nullptr;
}

//  --------------------------------------------------------------------------
//  Return contact of the asset
char* topologyresolver_to_contact(topologyresolver_t* self)
{
    if (self && self->iname) {
        fty_proto_t* rc_message = static_cast<fty_proto_t*>(zhashx_lookup(self->assets, self->iname));
        if (rc_message) {
            const char* contact_email = fty_proto_ext_string(rc_message, "contact_email", nullptr);
            if (contact_email) {
                return strdup(contact_email);
            }
        }
    }
    return nullptr;
}

//  --------------------------------------------------------------------------
//  Return topology as string of friendly names (or nullptr if incomplete)
char* topologyresolver_to_string(topologyresolver_t* self, const char* separator)
{
    zlistx_t* parents = topologyresolver_to_list(self);

    if (zlistx_size(parents) == 0) {
        zlistx_destroy(&parents);
        return nullptr;
    }

    zstr_free(&self->topology);
    self->topology = strdup("");

    char* iname    = static_cast<char*>(zlistx_first(parents));
    while (iname) {
        fty_proto_t* msg = static_cast<fty_proto_t*>(zhashx_lookup(self->assets, iname));
        if (msg) {
            const char* ename = fty_proto_ext_string(msg, "name", "" /*iname*/);
            char*       tmp   = zsys_sprintf("%s%s%s", self->topology, ename, separator);
            if (tmp) {
                zstr_free(&self->topology);
                self->topology = tmp;
            }
        }
        iname = static_cast<char*>(zlistx_next(parents));
    }

    if (strlen(self->topology) >= strlen(separator)) {
        // remove trailing separator
        char* p = &self->topology[strlen(self->topology) - strlen(separator)];
        *p      = 0;
    }

    zlistx_destroy(&parents);

    return strdup(self->topology);
}

//  --------------------------------------------------------------------------
//  Return zlist of inames starting with asset up to DC
//  Empty list is returned if the topology is incomplete yet
zlistx_t* topologyresolver_to_list(topologyresolver_t* self)
{
    zlistx_t* list = zlistx_new();
    zlistx_set_destructor(list, reinterpret_cast<void (*)(void**)>(zstr_free));
    zlistx_set_duplicator(list, reinterpret_cast<void* (*)(const void*)>(strdup));
    zlistx_set_comparator(list, reinterpret_cast<int (*)(const void*, const void*)>(strcmp));

    if (!self || !self->iname) {
        return list;
    }

    fty_proto_t* msg = static_cast<fty_proto_t*>(zhashx_lookup(self->assets, self->iname));
    if (!msg) {
        return list;
    }

    char buffer[16]; // strlen ("parent_name.123") + 1

    for (int i = 1; i < 100; i++) {
        snprintf(buffer, sizeof(buffer), "parent_name.%d", i);
        const char* parent = fty_proto_aux_string(msg, buffer, nullptr);
        if (!parent) {
            break;
        }

        bool purgeAndBreak = false; // on error

        if (!zhashx_lookup(self->assets, parent)) {
            if (mlm_client_connected(self->client)) {
                // ask ASSET_AGENT for ASSET_DETAIL
                zuuid_t* uuid = zuuid_new();
                const char* uuid_sent = zuuid_str_canonical(uuid);

                log_debug("ask %s for ASSET_DETAIL, RC = %s, iname = %s", AGENT_FTY_ASSET, self->iname, parent);
                zmsg_t* reply = NULL;
                int r = mlm_client_sendtox(self->client, AGENT_FTY_ASSET, "ASSET_DETAIL", "GET", uuid_sent, parent, nullptr);
                if (r != 0) {
                    log_error("sendto %s ASSET_DETAIL %s failed (r : %d)", AGENT_FTY_ASSET, parent, r);
                }
                else {
                    zpoller_t* poller = zpoller_new(mlm_client_msgpipe(self->client), NULL);
                    reply = (poller && zpoller_wait(poller, 5000)) ? mlm_client_recv(self->client) : NULL;
                    zpoller_destroy(&poller);
                    if (!reply) {
                        log_error("reply %s ASSET_DETAIL %s is empty", AGENT_FTY_ASSET, parent);
                    }
                }

                if (reply) {
                    char* uuid_recv = zmsg_popstr(reply);
                    if (uuid_recv && streq(uuid_recv, uuid_sent) && fty_proto_is(reply)) {
                        fty_proto_t* parent_fmsg = fty_proto_decode(&reply);
                        zhashx_update(self->assets, parent, parent_fmsg);
                        zlistx_add_start(list, const_cast<char*>(parent));
                    }
                    else {
                        // invalid uuid or unknown parent, topology is not complete
                        log_error("reply %s ASSET_DETAIL %s invalid uuid/reply", AGENT_FTY_ASSET, parent);
                        purgeAndBreak = true;
                    }
                    zstr_free(&uuid_recv);
                }
                else {
                    // parent is unknown, topology is not complete
                    purgeAndBreak = true;
                }

                zmsg_destroy(&reply);
                zuuid_destroy(&uuid);
            }
            else {
                // parent is unknown, topology is not complete
                purgeAndBreak = true;
            }
        }
        else {
            zlistx_add_start(list, const_cast<char*>(parent));
        }

        if (purgeAndBreak) {
            zlistx_purge(list); // emptied
            break;
        }
    }

    return list;
}
