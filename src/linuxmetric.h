/*  =========================================================================
    linuxmetric - Class for finding out Linux system info

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

#define LINUXMETRIC_UPTIME          "uptime"
#define LINUXMETRIC_CPU_USAGE       "usage.cpu"
#define LINUXMETRIC_CPU_TEMPERATURE "temperature.cpu"
#define LINUXMETRIC_MEMORY_TOTAL    "total.memory"
#define LINUXMETRIC_MEMORY_USED     "used.memory"
#define LINUXMETRIC_MEMORY_USAGE    "usage.memory"
#define LINUXMETRIC_DATA0_TOTAL     "total.data.0"
#define LINUXMETRIC_DATA0_USED      "used.data.0"
#define LINUXMETRIC_DATA0_USAGE     "usage.data.0"
#define LINUXMETRIC_SYSTEM_TOTAL    "total.system"
#define LINUXMETRIC_SYSTEM_USED     "used.system"
#define LINUXMETRIC_SYSTEM_USAGE    "usage.system"

#define BANDWIDTH_TEMPLATE   "%s_bandwidth.%s"
#define BYTES_TEMPLATE       "%s_bytes.%s"
#define ERROR_RATIO_TEMPLATE "%s_error_ratio.%s"

struct _linuxmetric_t
{
    char*       type;
    double      value;
    const char* unit;
};

typedef struct _linuxmetric_t linuxmetric_t;

//  Create a new linuxmetric
linuxmetric_t* linuxmetric_new(void);

//  Destroy the linuxmetric
void linuxmetric_destroy(linuxmetric_t** self_p);

// Create zlistx containing all Linux system info
zlistx_t* linuxmetric_get_all(int interval, zhashx_t* history, const std::string& root_dir, bool metrics_test);

zhashx_t* linuxmetric_list_interfaces(const std::string& root_dir);
