/*  =========================================================================
    ftyinfo - Class for keeping fty information

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
    ftyinfo - Class for keeping fty information
@discuss
@end
*/

#include "ftyinfo.h"
#include "fty_info.h"
#include "cxxtools/serializationinfo.h"
#include <fstream>
#include <fty_log.h>
#include <istream>
#include <map>
#include <set>
#include <fty_common_json.h>

extern const char* EV_DATA_DIR; 

static int s_calendar_to_datetime(time_t timestamp, char* buffer, size_t n)
{
    struct tm* tmp = gmtime(&timestamp);
    if (!tmp || strftime(buffer, n, "%FT%TZ", tmp) == 0) { // it's safe to check for 0, since we expect non-zero string
        return -1;
    }
    return 0;
}

static char* s_get_accepted_license_file(void)
{
    char* accepted_license = NULL;
    char* env              = getenv(EV_DATA_DIR);

    if (asprintf(&accepted_license, "%s/license", env ? env : "/var/lib/fty") == -1) {
        return NULL;
    }
    return accepted_license;
}

static void s_get_installation_date(const std::string& file, std::string& installation_date)
{
    try {
        std::ifstream finput(file);
        if (finput.is_open() && finput.good()) {
            // We assume the second line is license acceptance unix time
            std::getline(finput, installation_date);
            std::getline(finput, installation_date);
            int64_t i64 = std::stoll(installation_date);
            // TODO: we should probably check for time_t max/min, but hey!... ;)
            char chtmp[64];
            if (s_calendar_to_datetime(i64, chtmp, 64) == -1) {
                installation_date = "N/A - Error retrieving installation date";
                throw std::runtime_error("calendar_to_datetime () failed.");
            }
            installation_date.assign(chtmp);
        }
    } catch (const std::exception& e) {
        installation_date = "N/A - Undefined error occured";
        log_error("Exception caught: %s", e.what());
    }
}

static const char* RELEASE_DETAILS = "/etc/release-details.json";
static const char* BRANDING_INFO   = "/etc/etn-ipm2-branding.conf";

static cxxtools::SerializationInfo* s_load_release_details()
{
    cxxtools::SerializationInfo* si = new cxxtools::SerializationInfo();
    try {
        JSON::readFromFile(RELEASE_DETAILS, *si);
        log_info("fty-info:load %s OK", RELEASE_DETAILS);
    } catch (const std::exception& e) {
        log_error("Error while parsing JSON: %s", e.what());
    }
    return si;
}

static char* s_get_release_details(cxxtools::SerializationInfo& si, const char* key, const char* dfl)
{
    try {
        std::string value;
        si.getMember("release-details").getMember(key) >>= value;
        return strdup(value.c_str());
    } catch (const std::exception& e) {
        log_error("Problem with getting %s in JSON: %s", key, e.what());
    }
    return dfl ? strdup(dfl) : NULL;
}

static cxxtools::SerializationInfo* s_load_branding_info()
{
    cxxtools::SerializationInfo* si = new cxxtools::SerializationInfo();
    try {
        JSON::readFromFile(BRANDING_INFO, *si);
        log_info("fty-info:load %s OK", BRANDING_INFO);
    } catch (const std::exception& e) {
        log_error("Error while parsing JSON: %s", e.what());
    }
    return si;
}

static char* s_get_branding_info(cxxtools::SerializationInfo& si, const char* key, const char* dfl)
{
    try {
        std::string value;
        si.getMember(key) >>= value;
        return strdup(value.c_str());
    } catch (const std::exception& e) {
        log_error("Problem with getting %s in JSON: %s", key, e.what());
    }
    return dfl ? strdup(dfl) : NULL;
}

//  --------------------------------------------------------------------------
//  Create a new ftyinfo

ftyinfo_t* ftyinfo_new(topologyresolver_t* resolver, const char* path)
{
    ftyinfo_t* self = static_cast<ftyinfo_t*>(zmalloc(sizeof(ftyinfo_t)));

    self->infos = zhash_new();

    // set hostname
    char hostname[HOST_NAME_MAX + 1];
    int rv = gethostname(hostname, sizeof(hostname));
    if (rv == -1) {
        log_warning("ftyinfo could not be fully initialized (error while getting the hostname)");
        self->hostname = strdup("localhost");
    } else {
        self->hostname = strdup(hostname);
    }
    log_info("fty-info:hostname  = '%s'", self->hostname);

    // set id
    self->id = topologyresolver_id(resolver);
    log_info("fty-info:id        = '%s'", self->id);

    // set name
    self->name = topologyresolver_to_rc_name(resolver);
    log_info("fty-info:name      = '%s'", self->name);

    // set name_uri
    self->name_uri = topologyresolver_to_rc_name_uri(resolver);
    log_info("fty-info:name_uri  = '%s'", self->name_uri);

    // set location
    self->location = topologyresolver_to_string(resolver, ">");
    log_info("fty-info:location  = '%s'", self->location);

    // set parent_uri
    self->parent_uri = topologyresolver_to_parent_uri(resolver);
    log_info("fty-info:parent_uri= '%s'", self->parent_uri);

    // set uuid, vendor, product, part_number, verson from /etc/release-details.json
    cxxtools::SerializationInfo* si = s_load_release_details();
    if (si) {
        self->uuid         = s_get_release_details(*si, "uuid", NULL);
        self->vendor       = s_get_release_details(*si, "hardware-vendor", NULL); // Eaton or OEMs
        self->manufacturer = strdup("EATON");                                     // Eaton only
        self->serial       = s_get_release_details(*si, "hardware-serial-number", "N/A");
        self->product      = s_get_release_details(*si, "hardware-catalog-number", NULL);
        self->part_number  = s_get_release_details(*si, "hardware-part-number", NULL);
        self->version      = s_get_release_details(*si, "osimage-name", NULL);
        log_info("fty-info:uuid         = '%s'", self->uuid);
        log_info("fty-info:vendor       = '%s'", self->vendor);
        log_info("fty-info:manufacturer = '%s'", self->manufacturer);
        log_info("fty-info:serial       = '%s'", self->serial);
        log_info("fty-info:product      = '%s'", self->product);
        log_info("fty-info:part_number  = '%s'", self->part_number);
        log_info("fty-info:version      = '%s'", self->version);
    }

    // get complementary branding info
    cxxtools::SerializationInfo* bi = s_load_branding_info();
    if (bi) {
        self->licensing_portal          = s_get_branding_info(*bi, "licensing_portal", "N/A");
        log_info("fty-info:licensing_portal = '%s'", self->licensing_portal);
    }

    // set description, contact
    self->description = topologyresolver_to_description(resolver);
    self->contact     = topologyresolver_to_contact(resolver);
    log_info("fty-info:description     = '%s'", self->description);
    log_info("fty-info:contact         = '%s'", self->contact);

    // set installDate
    char*       license = s_get_accepted_license_file();
    std::string datetime;
    s_get_installation_date(license, datetime);
    self->installDate = strdup(datetime.c_str());
    log_info("fty-info:installDate     = '%s'", self->installDate);
    zstr_free(&license);

    // use default
    self->path            = strdup(path);
    self->protocol_format = strdup(TXT_PROTO_FORMAT);

    // update type (ipm-va by default)
    std::string s_type = TXT_IPM_VA_TYPE;
    if (self->product) {
        if (streq(self->product, "IPC3000")) {
            s_type = TXT_IPC_TYPE;
        } else if (streq(self->product, "IPM Editions VA")) {
            s_type = TXT_IPM_VA_TYPE;
        } else if (streq(self->product, "IPM Infra VA") || streq(self->product, "IPC3000E-LXC")) {
            s_type = TXT_IPC_VA_TYPE;
        }
    }
    self->type    = strdup(s_type.c_str());
    self->txtvers = strdup(TXT_VER);

    log_info("fty-info:path            = '%s'", self->path);
    log_info("fty-info:protocol_format = '%s'", self->protocol_format);
    log_info("fty-info:type            = '%s'", self->type);
    log_info("fty-info:txtvers         = '%s'", self->txtvers);

    // search for IPv4 addresses
    const int IP_SIZE = int(sizeof(self->ip) / sizeof(self->ip[0]));
    for (int i = 0; i < IP_SIZE; i++) {
        self->ip[i] = NULL;
    }
    struct ifaddrs *interfaces = NULL;
    if (getifaddrs(&interfaces) != -1) {
        char host[NI_MAXHOST];
        int counter = 0;
        for (struct ifaddrs *iface = interfaces; iface != NULL; iface = iface->ifa_next) {
            if (iface->ifa_addr == NULL)
                continue;
            // here we support IPv4 only, only get first IP_SIZE addresses
            if (iface->ifa_addr->sa_family == AF_INET
                && 0 == getnameinfo(iface->ifa_addr, sizeof(struct sockaddr_in),
                        host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST)
            ) {
                self->ip[counter++] = strdup(host);
            }
            if (counter == IP_SIZE) {
                break;
            }
        }
        freeifaddrs(interfaces);
    }

    if (si)
        delete si;
    if (bi)
        delete bi;

    return self;
}

//  --------------------------------------------------------------------------
//  Create a new ftyinfo for tests

ftyinfo_t* ftyinfo_test_new(void)
{
    ftyinfo_t* self = static_cast<ftyinfo_t*>(zmalloc(sizeof(ftyinfo_t)));
    // TXT attributes
    self->infos            = zhash_new();
    self->id               = strdup(TST_ID);
    self->uuid             = strdup(TST_UUID);
    self->hostname         = strdup(TST_HOSTNAME);
    self->name             = strdup(TST_NAME);
    self->name_uri         = strdup(TST_NAME_URI);
    self->product          = strdup(TST_PRODUCT);
    self->vendor           = strdup(TST_VENDOR);
    // self->manufacturer is skipped!?
    self->licensing_portal = strdup(TST_LIC_URL);
    self->serial           = strdup(TST_SERIAL);
    self->part_number      = strdup(TST_PART_NUMBER);
    self->location         = strdup(TST_LOCATION);
    self->parent_uri       = strdup(TST_PARENT_URI);
    self->version          = strdup(TST_VERSION);
    self->description      = strdup(TST_DESCRIPTION);
    self->contact          = strdup(TST_CONTACT);
    self->installDate      = strdup(TST_INSTALL_DATE);
    self->path             = strdup(DEFAULT_PATH);
    self->protocol_format  = strdup(TXT_PROTO_FORMAT);
    self->type             = strdup(TXT_IPC_TYPE);
    self->txtvers          = strdup(TXT_VER);
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the ftyinfo

void ftyinfo_destroy(ftyinfo_t** self_ptr)
{
    if (!self_ptr)
        return;
    if (*self_ptr) {
        ftyinfo_t* self = *self_ptr;

        // Free class properties here
        zhash_destroy(&self->infos);

        zstr_free(&self->id);
        zstr_free(&self->uuid);
        zstr_free(&self->hostname);
        zstr_free(&self->name);
        zstr_free(&self->name_uri);
        zstr_free(&self->product);
        zstr_free(&self->vendor);
        zstr_free(&self->manufacturer);
        zstr_free(&self->licensing_portal);
        zstr_free(&self->serial);
        zstr_free(&self->part_number);
        zstr_free(&self->location);
        zstr_free(&self->parent_uri);
        zstr_free(&self->version);
        zstr_free(&self->description);
        zstr_free(&self->contact);
        zstr_free(&self->installDate);
        zstr_free(&self->path);
        zstr_free(&self->protocol_format);
        zstr_free(&self->type);
        zstr_free(&self->txtvers);

        const int IP_SIZE = int(sizeof(self->ip) / sizeof(self->ip[0]));
        for (int i = 0; i < IP_SIZE; i++) {
            zstr_free(&self->ip[i]);
        }
 
        // Free object itself
        free(self);
        *self_ptr = NULL;
    }
}

//  --------------------------------------------------------------------------
//  getters

const char* ftyinfo_uuid(ftyinfo_t* self)
{
    return self ? self->uuid : NULL;
}

const zhash_t* ftyinfo_infohash(ftyinfo_t* self)
{
    if (!self)
        return NULL;

    zhash_destroy(&self->infos);
    self->infos = zhash_new();

    if (self->id)
        zhash_insert(self->infos, INFO_ID, self->id);
    if (self->uuid)
        zhash_insert(self->infos, INFO_UUID, self->uuid);
    if (self->hostname)
        zhash_insert(self->infos, INFO_HOSTNAME, self->hostname);
    if (self->name)
        zhash_insert(self->infos, INFO_NAME, self->name);
    if (self->name_uri)
        zhash_insert(self->infos, INFO_NAME_URI, self->name_uri);
    if (self->vendor)
        zhash_insert(self->infos, INFO_VENDOR, self->vendor);
    if (self->licensing_portal)
        zhash_insert(self->infos, INFO_LICENSING_PORTAL, self->licensing_portal);
    if (self->manufacturer)
        zhash_insert(self->infos, INFO_MANUFACTURER, self->manufacturer);
    if (self->product)
        zhash_insert(self->infos, INFO_PRODUCT, self->product);
    if (self->serial)
        zhash_insert(self->infos, INFO_SERIAL, self->serial);
    if (self->part_number)
        zhash_insert(self->infos, INFO_PART_NUMBER, self->part_number);
    if (self->location)
        zhash_insert(self->infos, INFO_LOCATION, self->location);
    if (self->parent_uri)
        zhash_insert(self->infos, INFO_PARENT_URI, self->parent_uri);
    if (self->version)
        zhash_insert(self->infos, INFO_VERSION, self->version);
    if (self->description)
        zhash_insert(self->infos, INFO_DESCRIPTION, self->description);
    if (self->contact)
        zhash_insert(self->infos, INFO_CONTACT, self->contact);
    if (self->installDate)
        zhash_insert(self->infos, INFO_INSTALL_DATE, self->installDate);
    if (self->path)
        zhash_insert(self->infos, INFO_REST_PATH, self->path);
    if (self->protocol_format)
        zhash_insert(self->infos, INFO_PROTOCOL_FORMAT, self->protocol_format);
    if (self->type)
        zhash_insert(self->infos, INFO_TYPE, self->type);
    if (self->txtvers)
        zhash_insert(self->infos, INFO_TXTVERS, self->txtvers);
    if (self->ip[0])
        zhash_insert(self->infos, INFO_IP1, self->ip[0]);
    if (self->ip[1])
        zhash_insert(self->infos, INFO_IP2, self->ip[1]);
    if (self->ip[2])
        zhash_insert(self->infos, INFO_IP3, self->ip[2]);

    return self->infos;
}
