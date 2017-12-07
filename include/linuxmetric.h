/*  =========================================================================
    linuxmetric - Class for finding out Linux system info

    Copyright (C) 2014 - 2017 Eaton

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

#ifndef LINUXMETRIC_H_INCLUDED
#define LINUXMETRIC_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#define LINUXMETRIC_UPTIME "uptime"
#define LINUXMETRIC_CPU_USAGE "usage.cpu"
#define LINUXMETRIC_CPU_TEMPERATURE "temperature.cpu"
#define LINUXMETRIC_MEMORY_TOTAL "total.memory"
#define LINUXMETRIC_MEMORY_USED "used.memory"
#define LINUXMETRIC_MEMORY_USAGE "usage.memory"
#define LINUXMETRIC_SDCARD_TOTAL "total.sd-card"
#define LINUXMETRIC_SDCARD_USED "used.sd-card"
#define LINUXMETRIC_SDCARD_USAGE "usage.sd-card"
#define LINUXMETRIC_FLASH_TOTAL "total.flash"
#define LINUXMETRIC_FLASH_USED  "used.flash"
#define LINUXMETRIC_FLASH_USAGE "usage.flash"

#define BANDWIDTH_TEMPLATE "%s_bandwidth.%s"
#define BYTES_TEMPLATE "%s_bytes.%s"
#define ERROR_RATIO_TEMPLATE "%s_error_ratio.%s"

struct _linuxmetric_t {
    char *type;
    double value;
    const char *unit;
};

//  @interface
//  Create a new linuxmetric
FTY_INFO_EXPORT linuxmetric_t *
    linuxmetric_new (void);

//  Destroy the linuxmetric
FTY_INFO_EXPORT void
    linuxmetric_destroy (linuxmetric_t **self_p);

// Create zlistx containing all Linux system info
FTY_INFO_EXPORT zlistx_t *
    linuxmetric_get_all
    (int interval,
     zhashx_t *history,
     std::string &root_dir,
     bool metrics_test);

FTY_INFO_EXPORT zhashx_t *
    linuxmetric_list_interfaces (std::string &root_dir);
//  @end

#ifdef __cplusplus
}
#endif

#endif
