/*  =========================================================================
    fty-info - Agent which returns rack controller information

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

#pragma once
#include <czmq.h>
#include <string>

//  Add your own public definitions here, if you need them
#define FTY_INFO_AGENT                        "fty-info"
#define FTY_ASSET_AGENT                       "asset-agent"
#define FTY_INFO_CMD                          "INFO"
#define DEFAULT_PATH                          "/api/v1/admin/info"
#define DEFAULT_ANNOUNCE_INTERVAL_SEC         60
#define DEFAULT_LINUXMETRICS_INTERVAL_SEC     30
#define STR_DEFAULT_LINUXMETRICS_INTERVAL_SEC "30"

// TODO: get from config
#define TIMEOUT_MS            -1                                     // wait infinitely
#define DEFAULT_UUID          "00000000-0000-0000-0000-000000000000" // in case of UUID being NULL
#define DEFAULT_RC_INAME      "rackcontroller-0"
#define INFO_ID               "id"
#define INFO_UUID             "uuid"
#define INFO_HOSTNAME         "hostname"
#define INFO_NAME             "name"
#define INFO_NAME_URI         "name-uri"
#define INFO_VENDOR           "vendor"
#define INFO_LICENSING_PORTAL "licensing_portal"
#define INFO_MANUFACTURER     "manufacturer"
#define INFO_PRODUCT          "product"
#define INFO_SERIAL           "serialNumber"
#define INFO_PART_NUMBER      "partNumber"
#define INFO_LOCATION         "location"
#define INFO_PARENT_URI       "parent-uri"
#define INFO_VERSION          "version"
#define INFO_DESCRIPTION      "description"
#define INFO_CONTACT          "contact"
#define INFO_INSTALL_DATE     "installDate"
#define INFO_REST_PATH        "path"
#define INFO_REST_PORT        "port"
#define INFO_PROTOCOL_FORMAT  "protocol-format"
#define INFO_TYPE             "type"
#define INFO_TXTVERS          "txtvers"
#define INFO_IP1              "ip.1"
#define INFO_IP2              "ip.2"
#define INFO_IP3              "ip.3"

// Config file accessors
const char* s_get(zconfig_t* config, const char* key, const char* dfl);
