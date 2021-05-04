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

/*
@header
    linuxmetric - Class for finding out Linux system info
@discuss
@end
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <limits>
#include <sys/statvfs.h>
#include <cxxtools/directory.h>

#include "fty_info_classes.h"


///////////////////////////////////////////
// Static functions which parse /proc files
//////////////////////////////////////////

// Get n-th field of the string (counted from 1), which can be parsed as double
static double
s_get_field (std::string line, int index)
{
        std::istringstream stream (line);
        int i = 1;
        std::string field;
        stream.exceptions ( std::istringstream::failbit | std::istringstream::badbit );
        try {
            while (!stream.eof () && i <= index) {
                stream >> field;
                i++;
            }
            return std::stod (field);
        }
        catch (std::istringstream::failure& e) {
            log_error ("Error while parsing string %s", line.c_str ());
            return std::numeric_limits<double>::quiet_NaN ();
        }
        catch (std::invalid_argument& e) {
            log_error ("Requested field %s is not a double", field.c_str ());
            return std::numeric_limits<double>::quiet_NaN ();
        }
        catch (std::out_of_range& e) {
            log_error ("Requested field %s is out of range", field.c_str ());
            return std::numeric_limits<double>::quiet_NaN ();
        }
}


// Get line number n (counted from 1)
static std::string
s_getline_by_number (std::string filename, int index)
{
    std::ifstream file (filename, std::ifstream::in);
    if (!file) {
        log_error ("Could not open '%s'", filename.c_str ());
        return "";
    }

    file.exceptions ( std::ifstream::failbit | std::ifstream::badbit );
    try {
        int i = 1;
        std::string line;
        // ignore first (n-1) lines
        while (!file.eof () && i < index) {
            file.ignore (std::numeric_limits<std::streamsize>::max (), '\n');
            i++;
        }
        std::getline (file, line);
        return line;
    }
    catch (std::ifstream::failure& e) {
        log_error ("Error while reading file %s", filename.c_str ());
        return "";
    }
}

// Get line starting with <name>
static std::string
s_getline_by_name (std::string filename, const char *name)
{
    std::ifstream file (filename, std::ifstream::in);
    if (!file) {
        log_error ("Could not open '%s'", filename.c_str ());
        return "";
    }

    file.exceptions ( std::ifstream::failbit | std::ifstream::badbit );
    try {
        std::string line;
        do {
            std::getline (file, line);
        } while (!file.eof () && strncmp (line.c_str (), name, strlen (name)) != 0);
        return line;
    }
    catch (std::ifstream::failure& e) {
        log_error ("Error while reading file %s", filename.c_str ());
        return "";
    }
}

static double
s_round (double d)
{
    return (d - floor(d) > 0.5) ? ceil(d) : floor(d);
}

////////////////////////////////////////////////////////////
// Static functions which get metrics values
// All magical constants can be found in /proc and /sys documentation.
////////////////////////////////////////////////////////////

static linuxmetric_t *
s_uptime (std::string &root_dir)
{
    std::string line = s_getline_by_number (root_dir + "proc/uptime", 1);
    double uptime = s_get_field (line, 1);

    linuxmetric_t *uptime_info = linuxmetric_new ();
    uptime_info->type = strdup (LINUXMETRIC_UPTIME);
    uptime_info->value = s_round (uptime);
    uptime_info->unit = "sec";

    return uptime_info;
}

static linuxmetric_t *
s_cpu_usage (std::string &root_dir, zhashx_t *history)
{
    std::string line_cpu = s_getline_by_name (root_dir + "proc/stat", "cpu");
    double user = s_get_field (line_cpu, 2);
    double nice = s_get_field (line_cpu, 3);
    double system = s_get_field (line_cpu, 4);
    double idle = s_get_field (line_cpu, 5);
    double iowait = s_get_field (line_cpu, 6);
    double irq = s_get_field (line_cpu, 7);
    double softirq = s_get_field (line_cpu, 8);
    double steal = s_get_field (line_cpu, 9);
    double numerator = idle + iowait;
    double denominator = user + nice + system + idle + iowait + irq + softirq + steal;
    double *history_numerator_ptr = static_cast<double *>(zhashx_lookup(history, HIST_CPU_NUMERATOR));
    double *history_denominator_ptr = static_cast<double *>(zhashx_lookup(history, HIST_CPU_DENOMINATOR));
    double history_numerator = 0;
    double history_denominator = 0;
    if (NULL != history_numerator_ptr && NULL != history_denominator_ptr) {
      history_numerator = *history_numerator_ptr;
      history_denominator = *history_denominator_ptr;
    }

    linuxmetric_t *cpu_usage_info = linuxmetric_new ();
    cpu_usage_info->type = strdup (LINUXMETRIC_CPU_USAGE);
    cpu_usage_info->value = s_round (100 - 100*((numerator - history_numerator)/(denominator - history_denominator)));
    cpu_usage_info->unit = "%";
    /* update or insert numerator and denominator to history */
    *history_numerator_ptr = numerator;
    *history_denominator_ptr = denominator;

    return cpu_usage_info;
}

static linuxmetric_t *
s_cpu_temperature (std::string &root_dir)
{
    std::string line = s_getline_by_number (root_dir + "sys/class/thermal/thermal_zone0/temp", 1);
    if (!line.empty ()) {
        double temperature = s_get_field (line, 1);

        linuxmetric_t *cpu_temperature_info = linuxmetric_new ();
        cpu_temperature_info->type = strdup (LINUXMETRIC_CPU_TEMPERATURE);
        cpu_temperature_info->value = s_round (temperature / 1000);
        cpu_temperature_info->unit = "C";
        return cpu_temperature_info;
    }
    return NULL;
}

static zlistx_t *
s_meminfo (std::string &root_dir)
{
    zlistx_t *meminfo = zlistx_new ();

    std::string line_total = s_getline_by_name (root_dir + "proc/meminfo", "MemTotal:");
    double memory_total = s_get_field (line_total, 2);

    linuxmetric_t *memory_total_info = linuxmetric_new ();
    memory_total_info->type = strdup (LINUXMETRIC_MEMORY_TOTAL);
    memory_total_info->value = memory_total;
    memory_total_info->unit = "kB";
    zlistx_add_end (meminfo, memory_total_info);

    std::string line_free = s_getline_by_name (root_dir + "proc/meminfo", "MemFree:");
    double memory_free = s_get_field (line_free, 2);
    std::string line_buffers = s_getline_by_name (root_dir + "proc/meminfo", "Buffers:");
    double memory_buffers = s_get_field (line_buffers, 2);
    std::string line_cached = s_getline_by_name (root_dir + "proc/meminfo", "Cached:");
    double memory_cached = s_get_field (line_cached, 2);
    std::string line_reclaim = s_getline_by_name (root_dir + "proc/meminfo", "SReclaimable:");
    double memory_reclaim = s_get_field (line_reclaim, 2);
    std::string line_shmem = s_getline_by_name (root_dir + "proc/meminfo", "Shmem:");
    double memory_shmem = s_get_field (line_shmem, 2);

    double memory_used = memory_total - memory_free - (memory_buffers + memory_cached + memory_reclaim - memory_shmem);

    linuxmetric_t *memory_used_info = linuxmetric_new ();
    memory_used_info->type = strdup (LINUXMETRIC_MEMORY_USED);
    memory_used_info->value = memory_used;
    memory_used_info->unit = "kB";
    zlistx_add_end (meminfo, memory_used_info);

    linuxmetric_t *memory_usage_info = linuxmetric_new ();
    memory_usage_info->type = strdup (LINUXMETRIC_MEMORY_USAGE);
    memory_usage_info->value = s_round (100 * (memory_used / memory_total));
    memory_usage_info->unit = "%";
    zlistx_add_end (meminfo, memory_usage_info);

    return meminfo;
}

static zlistx_t *
s_sdcard_info (std::string &root_dir)
{
    zlistx_t *sdcard_info = zlistx_new ();

    struct statvfs buf;
    std::string path (root_dir + "var/");
    statvfs (path.c_str (), &buf);
    int to_MB = 1024 * 1024;

    double sdcard_total = buf.f_blocks * buf.f_frsize;
    linuxmetric_t *sdcard_total_info = linuxmetric_new ();
    sdcard_total_info->type = strdup (LINUXMETRIC_DATA0_TOTAL);
    sdcard_total_info->value = s_round (sdcard_total / to_MB);
    sdcard_total_info->unit = "MB";
    zlistx_add_end (sdcard_info, sdcard_total_info);

    double sdcard_used = sdcard_total - buf.f_bsize * buf.f_bfree;
    linuxmetric_t *sdcard_used_info = linuxmetric_new ();
    sdcard_used_info->type = strdup (LINUXMETRIC_DATA0_USED);
    sdcard_used_info->value = s_round (sdcard_used / to_MB);
    sdcard_used_info->unit = "MB";
    zlistx_add_end (sdcard_info, sdcard_used_info);

    linuxmetric_t *sdcard_usage_info = linuxmetric_new ();
    sdcard_usage_info->type = strdup (LINUXMETRIC_DATA0_USAGE);
    sdcard_usage_info->value = s_round (100 * (sdcard_used / sdcard_total));
    sdcard_usage_info->unit = "%";
    zlistx_add_end (sdcard_info, sdcard_usage_info);

    return sdcard_info;
}

static zlistx_t *
s_flash_info (std::string &root_dir)
{
    zlistx_t *flash_info = zlistx_new ();

    struct statvfs buf;
    statvfs (root_dir.c_str (), &buf);
    int to_MB = 1024 * 1024;

    double flash_total = buf.f_blocks * buf.f_frsize;
    linuxmetric_t *flash_total_info = linuxmetric_new ();
    flash_total_info->type = strdup (LINUXMETRIC_SYSTEM_TOTAL);
    flash_total_info->value = s_round (flash_total / to_MB);
    flash_total_info->unit = "MB";
    zlistx_add_end (flash_info, flash_total_info);

    //df -h computes "/" usage from f_bavail, let's do the same
    double flash_used = flash_total - buf.f_bsize * buf.f_bavail;
    linuxmetric_t *flash_used_info = linuxmetric_new ();
    flash_used_info->type = strdup (LINUXMETRIC_SYSTEM_USED);
    flash_used_info->value = s_round (flash_used / to_MB);
    flash_used_info->unit = "MB";
    zlistx_add_end (flash_info, flash_used_info);

    linuxmetric_t *flash_usage_info = linuxmetric_new ();
    flash_usage_info->type = strdup (LINUXMETRIC_SYSTEM_USAGE);
    flash_usage_info->value = s_round (100 * (flash_used / flash_total));
    flash_usage_info->unit = "%";
    zlistx_add_end (flash_info, flash_usage_info);

    return flash_info;
}

static bool
is_interface_online (const char *interface, std::string &root_dir)
{
    std::string format (root_dir + "sys/class/net/%s");
    char *interface_dir = zsys_sprintf (format.c_str (), interface);
    // is the interface up?
    char *interface_state = zsys_sprintf ("%s/operstate", interface_dir);
    std::string state = s_getline_by_number (interface_state, 1);
    zstr_free (&interface_state);
    zstr_free (&interface_dir);
    return (state == "up");
}


static zlistx_t *
    s_network_usage
    (const char *interface,
     const char *direction,
     int interval,
     zhashx_t *history,
     std::string &root_dir)
{
    char *last_key = zsys_sprintf ("%s_%s_%s", NETWORK_HISTORY_PREFIX, direction, interface);
    double *value_last_ptr = static_cast<double *>(zhashx_lookup(history, last_key));
    double value_last = 0;
    if (NULL != value_last_ptr) {
        value_last = *value_last_ptr;
        log_trace ("%s:key found, value %lf", last_key, value_last);
    }

    zlistx_t *network_usage_info = zlistx_new ();

    std::string format (root_dir + "sys/class/net/%s/statistics/%s_bytes");
    char *path = zsys_sprintf (format.c_str (), interface, direction);
    std::string line = s_getline_by_number (path, 1);
    double bytes = s_get_field (line, 1);

    linuxmetric_t *bandwidth_info = linuxmetric_new ();
    char *bandwidth_type = zsys_sprintf (BANDWIDTH_TEMPLATE, direction, interface);
    bandwidth_info->type = strdup (bandwidth_type);
    bandwidth_info->value = s_round((bytes - value_last) / interval);
    bandwidth_info->unit = "Bps";
    zlistx_add_end (network_usage_info, bandwidth_info);

    linuxmetric_t *bytes_info = linuxmetric_new ();
    char *bytes_type = zsys_sprintf (BYTES_TEMPLATE, direction, interface);
    bytes_info->type = strdup (bytes_type);
    bytes_info->value = bytes;
    bytes_info->unit = "B";
    zlistx_add_end (network_usage_info, bytes_info);

    //store last value
    if (NULL == value_last_ptr) {
      value_last_ptr = (double *)zmalloc(sizeof(double));
      *value_last_ptr = bytes;
      zhashx_insert(history, last_key, value_last_ptr);
    } else {
      *value_last_ptr = bytes;
    }

    zstr_free (&bytes_type);
    zstr_free (&bandwidth_type);
    zstr_free (&path);
    zstr_free (&last_key);

    return network_usage_info;
}

static linuxmetric_t *
    s_network_error_ratio
    (const char *interface,
     const char *direction,
     zhashx_t *history,
     std::string &root_dir)
{
    char *last_errors_key = zsys_sprintf ("%s_%s_%s_errors", NETWORK_HISTORY_PREFIX, direction, interface);
    double *value_last_errors_ptr = (double *) zhashx_lookup(history, last_errors_key);
    double value_last_errors = 0;
    if (NULL != value_last_errors_ptr) {
        value_last_errors = *value_last_errors_ptr;
        log_trace ("%s:key found, value %lf", last_errors_key, value_last_errors);
    }

    char *last_packets_key = zsys_sprintf ("%s_%s_%s_packets", NETWORK_HISTORY_PREFIX, direction, interface);
    double *value_last_packets_ptr = (double *) zhashx_lookup(history, last_packets_key);
    double value_last_packets = 0;
    if (NULL != value_last_packets_ptr) {
        value_last_packets = *value_last_packets_ptr;
        log_trace ("%s:key found, value %lf", last_packets_key, value_last_packets);
    }

    std::string format_errors (root_dir + "sys/class/net/%s/statistics/%s_errors");
    char *errors_path = zsys_sprintf (format_errors.c_str (), interface, direction);
    std::string errors_line = s_getline_by_number (errors_path, 1);
    double errors = s_get_field (errors_line, 1);

    std::string format_packets (root_dir + "sys/class/net/%s/statistics/%s_packets");
    char *packets_path = zsys_sprintf (format_packets.c_str (), interface, direction);
    std::string packets_line = s_getline_by_number (packets_path, 1);
    double packets = s_get_field (packets_line, 1);

    linuxmetric_t *error_info = linuxmetric_new ();
    char *error_type = zsys_sprintf (ERROR_RATIO_TEMPLATE, direction, interface);
    error_info->type = strdup (error_type);
    error_info->value = s_round (100 * (errors - value_last_errors) / (packets - value_last_packets));
    error_info->unit = "%";

    //store last value
    if (NULL == value_last_errors_ptr) {
      value_last_errors_ptr = (double *)zmalloc(sizeof(double));
      *value_last_errors_ptr = errors;
      zhashx_insert(history, last_errors_key, value_last_errors_ptr);
    } else {
      *value_last_errors_ptr = errors;
    }
    //store last value
    if (NULL == value_last_packets_ptr) {
      value_last_packets_ptr = (double *)zmalloc(sizeof(double));
      *value_last_packets_ptr = packets;
      zhashx_insert(history, last_packets_key, value_last_packets_ptr);
    } else {
      *value_last_packets_ptr = packets;
    }

    zstr_free (&error_type);
    zstr_free (&packets_path);
    zstr_free (&errors_path);
    zstr_free (&last_errors_key);
    zstr_free (&last_packets_key);
    return error_info;
}

//  --------------------------------------------------------------------------
//  Create a new linuxmetric

linuxmetric_t *
linuxmetric_new (void)
{
    linuxmetric_t *self = static_cast<linuxmetric_t *>(zmalloc (sizeof (linuxmetric_t)));
    assert (self);
    //  Initialize class properties here
    return self;
}

//  --------------------------------------------------------------------------
//  Destroy the linuxmetric

void
linuxmetric_destroy (linuxmetric_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        linuxmetric_t *self = *self_p;
        //  Free class properties here
        zstr_free (&self->type);
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

zhashx_t *
linuxmetric_list_interfaces (std::string &root_dir)
{
    zhashx_t *interfaces = zhashx_new ();
    cxxtools::Directory dir(root_dir + "sys/class/net/");

    for (cxxtools::DirectoryIterator it = dir.begin (true); it != dir.end (); ++it) {
        std::string iface = *it;
        // we are not interested in loopback
        if (iface != "lo") {
            if (is_interface_online (iface.c_str (), root_dir))
                zhashx_update (interfaces, iface.c_str (), (void *) "up");
            else
                zhashx_update (interfaces, iface.c_str (), (void *) "down");
        }
    }

    return interfaces;
}

//--------------------------------------------------------------------------
//// Create zlistx containing all Linux system info

zlistx_t *
linuxmetric_get_all
    (int interval,
     zhashx_t *history,
     std::string &root_dir,
     bool metrics_test)
{
    zlistx_t *info = zlistx_new ();

    linuxmetric_t *uptime = s_uptime (root_dir);
    zlistx_add_end (info, uptime);
    linuxmetric_t *cpu_usage = s_cpu_usage (root_dir, history);
    zlistx_add_end (info, cpu_usage);
    linuxmetric_t *cpu_temperature = s_cpu_temperature (root_dir);
    if (cpu_temperature != NULL)
        zlistx_add_end (info, cpu_temperature);

    zlistx_t *meminfo = s_meminfo (root_dir);
    linuxmetric_t *mem_metric = static_cast<linuxmetric_t *>(zlistx_first (meminfo));
    while (mem_metric) {
        zlistx_add_end (info, mem_metric);
        mem_metric = static_cast<linuxmetric_t *>(zlistx_next (meminfo));
    }
    zlistx_destroy (&meminfo);

    if (!metrics_test) {
        zlistx_t *sdcard_info = s_sdcard_info (root_dir);
        linuxmetric_t *sdcard_metric = static_cast<linuxmetric_t *>(zlistx_first (sdcard_info));
        while (sdcard_metric) {
            zlistx_add_end (info, sdcard_metric);
            sdcard_metric = static_cast<linuxmetric_t *>(zlistx_next (sdcard_info));
        }
        zlistx_destroy (&sdcard_info);

        zlistx_t *flash_info = s_flash_info (root_dir);
        linuxmetric_t *flash_metric = static_cast<linuxmetric_t *>(zlistx_first (flash_info));
        while (flash_metric) {
            zlistx_add_end (info, flash_metric);
            flash_metric = static_cast<linuxmetric_t *>(zlistx_next (flash_info));
        }
        zlistx_destroy (&flash_info);
    }
    else {
        linuxmetric_t *sdcard_total_info = linuxmetric_new ();
        sdcard_total_info->type = strdup (LINUXMETRIC_DATA0_TOTAL);
        sdcard_total_info->value = 10;
        sdcard_total_info->unit = "MB";
        zlistx_add_end (info, sdcard_total_info);

        linuxmetric_t *sdcard_used_info = linuxmetric_new ();
        sdcard_used_info->type = strdup (LINUXMETRIC_DATA0_USED);
        sdcard_used_info->value = 1;
        sdcard_used_info->unit = "MB";
        zlistx_add_end (info, sdcard_used_info);

        linuxmetric_t *sdcard_usage_info = linuxmetric_new ();
        sdcard_usage_info->type = strdup (LINUXMETRIC_DATA0_USAGE);
        sdcard_usage_info->value = 100 * (sdcard_used_info->value / sdcard_total_info->value);
        sdcard_usage_info->unit = "%";
        zlistx_add_end (info, sdcard_usage_info);

        linuxmetric_t *flash_total_info = linuxmetric_new ();
        flash_total_info->type = strdup (LINUXMETRIC_SYSTEM_TOTAL);
        flash_total_info->value = 10;
        flash_total_info->unit = "MB";
        zlistx_add_end (info, flash_total_info);

        linuxmetric_t *flash_used_info = linuxmetric_new ();
        flash_used_info->type = strdup (LINUXMETRIC_SYSTEM_USED);
        flash_used_info->value = 5;
        flash_used_info->unit = "MB";
        zlistx_add_end (info, flash_used_info);

        linuxmetric_t *flash_usage_info = linuxmetric_new ();
        flash_usage_info->type = strdup (LINUXMETRIC_SYSTEM_USAGE);
        flash_usage_info->value = 100 * (flash_used_info->value / flash_total_info->value);
        flash_usage_info->unit = "%";
        zlistx_add_end (info, flash_usage_info);
    }

    // loop over all network interfaces
    zhashx_t *interfaces = linuxmetric_list_interfaces (root_dir);

    const char *state = (const char *) zhashx_first (interfaces);
    while (state != NULL)  {
        const char *iface = (const char *) zhashx_cursor (interfaces);
        log_trace ("interface %s = %s", iface, state);

        if (streq (state, "up")) {
            zlistx_t *rx = s_network_usage (iface, "rx", interval, history, root_dir);
            linuxmetric_t *network_usage_metric = static_cast<linuxmetric_t *>(zlistx_first (rx));
            while (network_usage_metric) {
                zlistx_add_end (info, network_usage_metric);
                network_usage_metric = static_cast<linuxmetric_t *>(zlistx_next (rx));
            }
            zlistx_destroy (&rx);

            zlistx_t *tx = s_network_usage (iface, "tx", interval, history, root_dir);
            network_usage_metric = static_cast<linuxmetric_t *>(zlistx_first (tx));
            while (network_usage_metric) {
                zlistx_add_end (info, network_usage_metric);
                network_usage_metric = static_cast<linuxmetric_t *>(zlistx_next (tx));
            }
            zlistx_destroy (&tx);

            linuxmetric_t *rx_error = s_network_error_ratio (iface, "rx", history, root_dir);
            if (rx_error != NULL)
                zlistx_add_end (info, rx_error);

            linuxmetric_t *tx_error = s_network_error_ratio (iface, "tx", history, root_dir);
            if (tx_error != NULL)
                zlistx_add_end (info, tx_error);
        }
        state = (const char *) zhashx_next (interfaces);
    }
    zhashx_destroy (&interfaces);
    return info;
}
