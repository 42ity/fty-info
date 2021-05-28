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

#pragma once
#include "topologyresolver.h"

// default values
#define SRV_IPC_NAME     "IPC"
#define SRV_TYPE         "_https._tcp."
#define SRV_STYPE        "_powerservice._sub._https._tcp."
#define SRV_PORT         "443"
#define TXT_PATH         "/api/v1/comm"
#define TXT_PROTO_FORMAT "etnrs"
#define TXT_IPC_TYPE     "ipc"
#define TXT_IPC_VA_TYPE  "ipc-va"
#define TXT_IPM_VA_TYPE  "ipm-va"
#define TXT_VER          "1"

// test value for INFO-TEST command reply
#define TST_ID            "rackcontroller-2"
#define TST_UUID          "ce7c523e-08bf-11e7-af17-080027d52c4f"
#define TST_HOSTNAME      "localhost"
#define TST_NAME          "MyIPC"
#define TST_INAME         "rackcontroller-0"
#define TST_NAME_URI      "/asset/rackcontroller-0"
#define TST_PRODUCT       "IPC3000"
#define TST_VENDOR        "Eaton"
#define TST_LIC_URL       "www.eaton.com/licensing-portal"
#define TST_SERIAL        "LA71026006"
#define TST_PART_NUMBER   "123456"
#define TST_LOCATION      "Rack1"
#define TST_PARENT_INAME  "rack-001"
#define TST_PARENT_URI    "/asset/rack-001"
#define TST_LOCATION2     "Rack2"
#define TST_PARENT2_INAME "rack-002"
#define TST_PARENT2_URI   "/asset/rack-002"
#define TST_VERSION       "1.0.0"
#define TST_DESCRIPTION   "Test IPC"
#define TST_CONTACT       "N.N."
#define TST_INSTALL_DATE  "2017-02-30T00:00:00Z"
#define TST_PATH          "/api/v1"
#define TST_PORT          "80"

// values within history
#define HIST_CPU_NUMERATOR     "cpu_usage_numerator"
#define HIST_CPU_DENOMINATOR   "cpu_usage_denominator"
#define NETWORK_HISTORY_PREFIX "network_history"

//  Structure of our class

struct _ftyinfo_t
{
    zhash_t* infos;
    char*    id;
    char*    uuid;
    char*    hostname;
    char*    name;
    char*    name_uri;
    char*    product;
    char*    vendor;
    char*    licensing_portal;
    char*    manufacturer;
    char*    serial;
    char*    part_number;
    char*    location;
    char*    parent_uri;
    char*    version;
    char*    description;
    char*    contact;
    char*    installDate;
    char*    path;
    char*    protocol_format;
    char*    type;
    char*    txtvers;
    char*    ip[3];
};

typedef struct _ftyinfo_t ftyinfo_t;

//  Create a new ftyinfo
ftyinfo_t* ftyinfo_new(topologyresolver_t* resolver, const char* path);

ftyinfo_t* ftyinfo_test_new(void);

//  Destroy the ftyinfo
void ftyinfo_destroy(ftyinfo_t** self_p);

// getters
const char* ftyinfo_uuid(ftyinfo_t* self);

zhash_t* ftyinfo_infohash(ftyinfo_t* self);
