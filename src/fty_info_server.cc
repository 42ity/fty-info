/*  =========================================================================
    fty_info_server - 42ity info server

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
    fty_info_server - 42ity info server
@discuss
@end
*/
#include "fty_info.h"
#include "ftyinfo.h"
#include "linuxmetric.h"
#include "topologyresolver.h"
#include <bits/local_lim.h>
#include <cxxtools/jsondeserializer.h>
#include <fstream>
#include <fty_log.h>
#include <fty_shm.h>
#include <ifaddrs.h>
#include <istream>
#include <czmq.h>
#include <malamute.h>
#include <map>
#include <set>
#include <string>
#include <unistd.h>

#define HW_CAP_FILE "42ity-capabilities.dsc"

struct fty_info_server_t
{
    //  Declare class properties here
    char*               name;
    char*               endpoint;
    char*               path;
    mlm_client_t*       client;
    mlm_client_t*       announce_client;
    bool                first_announce;
    bool                test;
    topologyresolver_t* resolver;
    int                 linuxmetrics_interval;
    std::string         root_dir; // directory to be considered / - used for testing
    zhashx_t*           history;
    char*               hw_cap_path;
};

// extern, this is kept for to handle with values set to ""
const char* s_get(zconfig_t* config, const char* key, const char* default_)
{
    const char* ret = config ? zconfig_get(config, key, default_) : NULL;
    if (ret && (*ret)) {
        return ret;
    }
    return default_;
}

//  --------------------------------------------------------------------------
//  Free wrapper for zhashx destructor
static void history_destructor(void** item)
{
    if (item && (*item)) {
        free(*item);
        *item = nullptr;
    }
}

//  --------------------------------------------------------------------------
//  Create a new fty_info_server

fty_info_server_t* info_server_new(char* name)
{
    fty_info_server_t* self = new fty_info_server_t; //root_dir is a std::string!!!
    assert(self);

    //  Initialize class properties here
    self->name            = strdup(name);
    self->client          = mlm_client_new();
    self->announce_client = mlm_client_new();
    self->first_announce  = true;
    self->test            = false;
    self->history         = zhashx_new();
    self->hw_cap_path     = NULL;
    self->resolver        = topologyresolver_new(DEFAULT_RC_INAME);

    double* numerator_ptr   = reinterpret_cast<double*>(zmalloc(sizeof(double)));
    double* denominator_ptr = reinterpret_cast<double*>(zmalloc(sizeof(double)));
    zhashx_set_destructor(self->history, history_destructor);
    zhashx_insert(self->history, HIST_CPU_NUMERATOR, numerator_ptr);
    zhashx_insert(self->history, HIST_CPU_DENOMINATOR, denominator_ptr);

    return self;
}

//  --------------------------------------------------------------------------
//  Destroy the fty_info_server

void info_server_destroy(fty_info_server_t** self_p)
{
    assert(self_p);
    if (*self_p) {
        fty_info_server_t* self = *self_p;
        //  Free class properties here
        mlm_client_destroy(&self->client);
        mlm_client_destroy(&self->announce_client);
        zstr_free(&self->name);
        zstr_free(&self->endpoint);
        zstr_free(&self->path);
        topologyresolver_destroy(&self->resolver);
        zhashx_destroy(&self->history);
        zstr_free(&self->hw_cap_path);
        //  Free object itself
        delete self;
        *self_p = NULL;
    }
}

// return NAME (uuid first 8 digits)
// the returned buffer must be freed by caller
static char* s_get_name(ftyinfo_t* info)
{
    std::string s_name = SRV_IPC_NAME;
    if (info && info->type && info->product
        && (strcmp(info->type, TXT_IPC_TYPE) != 0)
        && (strcmp(info->type, TXT_IPC_VA_TYPE) != 0))
    {
        s_name = info->product;
    }

    char first_digit[9];
    const char* uuid = ftyinfo_uuid(info);
    strncpy(first_digit, (uuid ? uuid : DEFAULT_UUID), 8);
    first_digit[8] = 0;

    char* buffer = NULL;
    asprintf(&buffer, "%s (%s)", s_name.c_str(), first_digit);

    return buffer;
}

// create INFO reply/publish message
//  body :
//    - INFO (command))
//    - name    IPC (12378)
//    - type    _https._tcp.
//    - subtype _powerservice._sub._https._tcp.
//    - port    443
//    - hashtable : TXT name, TXT value
//          id (internal id "rackcontroller-33")
//          uuid
//          name (meaning user-friendly name)
//          name_uri
//          vendor
//          serial
//          product
//          location
//          parent_uri
//          version
//          path
//          protocol format
//          type (meaning device type)
//          hostname
//          txtvers
static zmsg_t* s_create_info(ftyinfo_t* info)
{
    char* srv_name = s_get_name(info);

    zmsg_t* msg = zmsg_new();
    zmsg_addstr(msg, FTY_INFO_CMD);
    zmsg_addstr(msg, srv_name ? srv_name : DEFAULT_UUID);
    zmsg_addstr(msg, SRV_TYPE);
    zmsg_addstr(msg, SRV_STYPE);
    zmsg_addstr(msg, SRV_PORT);

    zstr_free(&srv_name);

    const zhash_t* map = ftyinfo_infohash(info);
    if (map) {
        zframe_t* frame_infos = zhash_pack(const_cast<zhash_t*>(map));
        zmsg_append(msg, &frame_infos);
        zframe_destroy(&frame_infos);
    }

    return msg;
}

//  --------------------------------------------------------------------------
//  publish INFO announcement on STREAM ANNOUNCE/ANNOUNCE-TEST
//  subject : CREATE/UPDATE
static void s_publish_announce(fty_info_server_t* self)
{
    if (!mlm_client_connected(self->announce_client))
        return;

    ftyinfo_t* info = (!self->test)
        ? ftyinfo_new(self->resolver, self->path)
        : ftyinfo_test_new();

    zmsg_t* msg = s_create_info(info);

    if (self->first_announce) {
        int r = mlm_client_send(self->announce_client, "CREATE", &msg);
        if (r != -1) {
            log_info("publish CREATE msg on ANNOUNCE STREAM");
            self->first_announce = false;
        }
        else {
            log_error("cant publish CREATE msg on ANNOUNCE STREAM");
        }
    }
    else {
        int r = mlm_client_send(self->announce_client, "UPDATE", &msg);
        if (r != -1) {
            log_info("publish UPDATE msg on ANNOUNCE STREAM");
        }
        else {
            log_error("cant publish UPDATE msg on ANNOUNCE STREAM");
        }
    }

    zmsg_destroy(&msg);
    ftyinfo_destroy(&info);
}

//  --------------------------------------------------------------------------
//  publish Linux system info on STREAM METRICS
static void s_publish_linuxmetrics(fty_info_server_t* self)
{
    char* rc_iname = topologyresolver_id(self->resolver);
    if (!rc_iname) {
        log_error("rc_iname is NULL");
        return;
    }

    zlistx_t* info = linuxmetric_get_all(self->linuxmetrics_interval, self->history, self->root_dir, self->test);
    if (!info) {
       log_error("info is NULL");
       free(rc_iname);
       return;
    }

    log_debug("s_publish_linuxmetrics for '%s' (info size: %zu)", rc_iname, (info ? zlistx_size(info) : 0));

    int ttl = 3 * self->linuxmetrics_interval; // in seconds
    linuxmetric_t* metric = static_cast<linuxmetric_t*>(zlistx_first(info));
    while (metric) {
        char* value = zsys_sprintf("%lf", metric->value);
        log_debug("Publishing metric %s, value %lf, unit %s", metric->type, metric->value, metric->unit);

        int r = fty::shm::write_metric(rc_iname, metric->type, value, metric->unit, ttl);
        if (r == 0) {
            log_trace("Metric %s published", metric->type);
        } else {
            log_error("Can't publish metric %s (r: %d)", metric->type, r);
        }
        linuxmetric_destroy(&metric);

        metric = static_cast<linuxmetric_t*>(zlistx_next(info));
        zstr_free(&value);
    }

    free(rc_iname);
    zlistx_destroy(&info);
}

//  --------------------------------------------------------------------------
//  process pipe message
//  return true means continue, false means TERM
static bool s_handle_pipe(fty_info_server_t* self, zmsg_t* message)
{
    bool ret = true;

    char* command = message ? zmsg_popstr(message) : NULL;
    log_debug("s_handle_pipe (command: %s)", command);

    if (!command) {
        log_warning("Empty command.");
    }
    else if (streq(command, "$TERM")) {
        log_info("Got $TERM");
        ret = false;
    }
    else if (streq(command, "CONNECT")) {
        char* endpoint = zmsg_popstr(message);

        if (endpoint) {
            if (!self->test)
                topologyresolver_set_endpoint(self->resolver, endpoint);
            zstr_free(&self->endpoint);
            self->endpoint = strdup(endpoint);
            log_debug("CONNECT: %s/%s", self->endpoint, self->name);
            int rv = mlm_client_connect(self->client, self->endpoint, 1000, self->name);
            if (rv == -1) {
                log_error("%s: mlm_client_connect failed", self->name);
            }
        }
        zstr_free(&endpoint);
    }
    else if (streq(command, "PATH")) {
        char* path = zmsg_popstr(message);

        if (path) {
            zstr_free(&self->path);
            self->path = strdup(path);
            log_debug("PATH: %s", self->path);
        }
        zstr_free(&path);
    }
    else if (streq(command, "CONSUMER")) {
        char* stream  = zmsg_popstr(message);
        char* pattern = zmsg_popstr(message);
        int   rv      = mlm_client_set_consumer(self->client, stream, pattern);
        if (rv == -1) {
            log_error("%s: can't set consumer on stream '%s', '%s'", self->name, stream, pattern);
        }
        zstr_free(&pattern);
        zstr_free(&stream);
    }
    else if (streq(command, "PRODUCER")) {
        char* stream = zmsg_popstr(message);
        if (streq(stream, "ANNOUNCE-TEST") || streq(stream, "ANNOUNCE")) {
            self->test = streq(stream, "ANNOUNCE-TEST");
            if (!self->test) {
                zmsg_t* republish = zmsg_new();
                int rv = mlm_client_sendto(self->client, FTY_ASSET_AGENT, "REPUBLISH", NULL, 5000, &republish);
                zmsg_destroy(&republish);
                if (rv != 0) {
                    log_error("%s: cannot send REPUBLISH message", self->name);
                }
                // no response expected
            }
            int rv = mlm_client_connect(self->announce_client, self->endpoint, 1000, "fty_info_announce");
            if (rv == -1) {
                log_error("fty_info_announce : mlm_client_connect failed\n");
            }
            rv = mlm_client_set_producer(self->announce_client, stream);
            if (rv == -1) {
                log_error("%s: can't set producer on stream '%s'", self->name, stream);
            }
            else { // do the first announce
                s_publish_announce(self);
            }
        }
        else if (streq(stream, "METRICS-TEST")) {
            // publish the first metrics
            // we need to keep this approach for testing purpose
            s_publish_linuxmetrics(self);
        }
        else {
            int rv = mlm_client_set_producer(self->client, stream);
            if (rv == -1) {
                log_error("%s: can't set producer on stream '%s'", self->name, stream);
            }
        }
        zstr_free(&stream);
    }
    else if (streq(command, "LINUXMETRICSINTERVAL")) {
        char* interval = zmsg_popstr(message);
        log_info("Will be publishing metrics each %s seconds", interval);
        self->linuxmetrics_interval = static_cast<int>(strtol(interval, NULL, 10));
        zstr_free(&interval);
    }
    else if (streq(command, "ROOT_DIR")) {
        char* root_dir = zmsg_popstr(message);
        log_info("Will be using %s as root dir for finding out Linux metrics", root_dir);
        self->root_dir.assign(root_dir ? root_dir : "");
        zstr_free(&root_dir);
    }
    else if (streq(command, "ANNOUNCE")) {
        s_publish_announce(self);
    }
    else if (streq(command, "LINUXMETRICS")) {
        s_publish_linuxmetrics(self);
    }
    else if (streq(command, "CONFIG")) {
        char* hw_cap_path = zmsg_popstr(message);
        zstr_free(&self->hw_cap_path);
        self->hw_cap_path = hw_cap_path;
        if (!self->hw_cap_path) {
            log_error("%s: %s hw_cap_path missing", self->name, command);
        }
    }
    else if (streq(command, "TEST")) {
        self->test = true;
    }
    else {
        log_error("%s: Unknown actor command (%s)", self->name, command);
    }

    zstr_free(&command);

    return ret;
}

//  fty message freefn prototype

void fty_msg_free_fn(void* data)
{
    if (!data)
        return;
    fty_proto_t* msg = static_cast<fty_proto_t*>(data);
    fty_proto_destroy(&msg);
}

//  --------------------------------------------------------------------------
// return zmsg_t with hw capability info or empty msg if info cannot be retrieved
static zmsg_t* s_hw_cap(fty_info_server_t* self, const char* type)
{
    if (!type) type = "(null)"; //secure

    zmsg_t* msg = zmsg_new();

    zconfig_t* cap = NULL;
    {
        char* path = zsys_sprintf("%s/%s", self->hw_cap_path, HW_CAP_FILE);
        log_debug("loading %s...", path);
        cap = zconfig_load(path);
        zstr_free(&path);
    }

    if (!cap) {
        log_error("cannot load capability file from %s/%s", self->hw_cap_path, HW_CAP_FILE);

        zmsg_addstr(msg, "ERROR");
        zmsg_addstr(msg, "cap does not exist");
    }
    else if (streq(type, "gpi") || streq(type, "gpo")) {
        char* path  = zsys_sprintf("hardware/%s/count", type);
        const char* count = s_get(cap, path, "");
        zstr_free(&path);

        zmsg_addstr(msg, "OK");
        zmsg_addstr(msg, type);
        zmsg_addstr(msg, count ? count : "0");

        if (!streq(count, "0")) {
            path = zsys_sprintf("hardware/%s/base_address", type);
            const char* ba = s_get(cap, path, "");
            zstr_free(&path);
            zmsg_addstr(msg, ba);

            path = zsys_sprintf("hardware/%s/offset", type);
            const char* offset = s_get(cap, path, "");
            zstr_free(&path);
            zmsg_addstr(msg, offset);

            path = zsys_sprintf("hardware/%s/mapping", type);
            zconfig_t* ret = zconfig_locate(cap, path);
            zstr_free(&path);

            if (ret) {
                ret = zconfig_child(ret);
                while (ret) {
                    zmsg_addstr(msg, zconfig_name(ret));
                    zmsg_addstr(msg, zconfig_value(ret));
                    ret = zconfig_next(ret);
                }
                zconfig_destroy(&ret); //always NULL here!?
            }
        }
    }
    else if (streq(type, "type")) {
        zmsg_addstr(msg, "OK");
        zmsg_addstr(msg, type);
        zmsg_addstr(msg, s_get(cap, "hardware/type", ""));
    }
    else {
        log_error("unsupported request for '%s'", type);

        zmsg_addstr(msg, "ERROR");
        zmsg_addstrf(msg, "unsupported type");
    }

    zconfig_destroy(&cap);

    return msg;
}

//  --------------------------------------------------------------------------
//  process message from FTY_PROTO_ASSET stream
void static s_handle_stream(fty_info_server_t* self, zmsg_t* message)
{
    if (!(message && fty_proto_is(message))) {
        return;
    }

    zmsg_t* aux = zmsg_dup(message);
    fty_proto_t* fproto = fty_proto_decode(&aux);
    zmsg_destroy(&aux);

    if (fproto && (fty_proto_id(fproto) == FTY_PROTO_ASSET)) {
        log_debug("s_handle_stream ASSET %s (%s)", fty_proto_name(fproto), fty_proto_operation(fproto));

        if (topologyresolver_asset(self->resolver, fproto)) {
            s_publish_announce(self);
        }
    }

    fty_proto_destroy(&fproto);
}

//  --------------------------------------------------------------------------
//  process message from MAILBOX DELIVER
//  bmsg request fty-info REQUEST INFO <uuid>
//  bmsg request fty-info REQUEST HW_CAP <uuid> <"type"|"gpi"|"gpo">

void static s_handle_mailbox(fty_info_server_t* self, zmsg_t* message)
{
    const char* sender = mlm_client_sender(self->client);
    char* command = message ? zmsg_popstr(message) : NULL;
    char* zuuid = message ? zmsg_popstr(message) : NULL;

    log_info("handle_mailbox (sender: %s, command: %s, zuuid: %s)", sender, command, zuuid);

    zmsg_t* reply = NULL;

    // we assume all request command are MAILBOX DELIVER, and with any subject"
    if (!command) {
        log_debug("Empty command");
    }
    else if (streq(command, "INFO")) {
        ftyinfo_t* info = ftyinfo_new(self->resolver, self->path);
        reply = s_create_info(info);
        ftyinfo_destroy(&info);
    }
    else if (streq(command, "HW_CAP")) {
        char* type = zmsg_popstr(message);
        reply = s_hw_cap(self, type);
        zstr_free(&type);
    }
    else if (streq(command, "INFO-TEST")) {
        ftyinfo_t* info = ftyinfo_test_new();
        reply = s_create_info(info);
        ftyinfo_destroy(&info);
    }
    else if (streq(command, "ERROR")) {
        // Don't reply to ERROR messages ?!
        log_warning("%s: Received ERROR command from '%s', ignoring", self->name, mlm_client_sender(self->client));
    }
    else {
        log_warning("%s: Received unexpected command '%s' from '%s'", self->name, command, mlm_client_sender(self->client));

        reply = zmsg_new();
        zmsg_addstr(reply, "ERROR");
        zmsg_addstrf(reply, "unexpected command");
    }

    if (reply) {
        zmsg_pushstrf(reply, "%s", zuuid ? zuuid : "missing-uuid"); // enforce reply w/ uuid

        int rv = mlm_client_sendto(self->client, sender, "info", NULL, 1000, &reply);
        if (rv != 0) {
            log_error("%s: failed to send reply to %s ", self->name, sender);
        }
    }

    zmsg_destroy(&reply);
    zstr_free(&zuuid);
    zstr_free(&command);
}

//  --------------------------------------------------------------------------
//  Create a new fty_info_server

void fty_info_server(zsock_t* pipe, void* args)
{
    char* name = static_cast<char*>(args);
    if (!name) {
        log_error("Address for fty-info actor is NULL");
        return;
    }

    fty_info_server_t* self = info_server_new(name);
    if (!self) {
        log_error("info_server_new() failed");
        return;
    }

    zpoller_t* poller = zpoller_new(pipe, mlm_client_msgpipe(self->client), NULL);
    if (!poller) {
        log_error("zpoller_new() failed");
        info_server_destroy(&self);
        return;
    }

    log_info("fty-info: Started");

    zsock_signal(pipe, 0);

    while (!zsys_interrupted) {
        void* which = zpoller_wait(poller, TIMEOUT_MS);
        if (which == NULL) {
            if (zpoller_terminated(poller) || zsys_interrupted) {
                break;
            }
        }
        else if (which == pipe) {
            log_trace("which == pipe");
            zmsg_t* message = zmsg_recv(pipe);
            bool res = s_handle_pipe(self, message);
            zmsg_destroy(&message);
            if (!res) {
                break; // $TERM
            }
        }
        else if (which == mlm_client_msgpipe(self->client)) {
            zmsg_t* message = mlm_client_recv(self->client);
            const char* command = mlm_client_command(self->client);
            if (streq(command, "STREAM DELIVER")) {
                s_handle_stream(self, message);
            }
            else if (streq(command, "MAILBOX DELIVER")) {
                s_handle_mailbox(self, message);
            }
            zmsg_destroy(&message);
        }
    }

    zpoller_destroy(&poller);
    info_server_destroy(&self);

    log_info("fty-info: Ended");
}
