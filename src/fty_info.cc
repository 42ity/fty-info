/*  =========================================================================
    fty_info - Agent which returns rack controller information

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

/// fty_info - Agent which returns rack controller information

#include "fty_info.h"
#include "fty_info_server.h"
#include "fty_info_rc0_runonce.h"

#include <fty_log.h>
#include <fty_proto.h>

#define RC0_RUNONCE_ACTOR "fty-info-rc0-runonce"
#define MLM_ENDPOINT "ipc://@/malamute"
#define HW_CAP_PATH "/usr/share/fty"

static int s_linuxmetrics_event(zloop_t* /*loop*/, int /*timer_id*/, void* output)
{
    zstr_send(output, "LINUXMETRICS");
    return 0;
}

static void usage()
{
    printf("fty-info [options] ...\n");
    printf("  -v|--verbose        verbose test output\n");
    printf("  -c|--config         path to config file\n");
    printf("  -e|--endpoint       malamute endpoint [ipc://@/malamute]\n");
    printf("  -h|--help           show this information\n");
}

int main(int argc, char* argv[])
{
    bool  verbose    = false;
    char* actor_name = NULL;
    char* endpoint   = NULL;
    char* path       = NULL;
    char* linuxmetrics_interval = NULL; // Linux metrics publishing interval (seconds)

    asprintf(&linuxmetrics_interval, "%d", DEFAULT_LINUXMETRICS_INTERVAL_SEC);

    // cleanup
    #define CLEANUP_STRINGS { \
        zstr_free(&actor_name); \
        zstr_free(&endpoint); \
        zstr_free(&path); \
        zstr_free(&linuxmetrics_interval); \
    }

    const char* config_file = NULL;

    // Parse command line
    for (int argn = 1; argn < argc; argn++) {
        const char* arg = argv[argn];
        const char* param = ((argn + 1) < argc) ? argv[argn + 1] : NULL;

        if (streq(arg, "-h") || streq(arg, "--help")) {
            usage();
            CLEANUP_STRINGS;
            return EXIT_SUCCESS;
        }
        else if (streq(arg, "-v") || streq(arg, "--verbose")) {
            verbose = true;
        }
        else if (streq(arg, "-c") || streq(arg, "--config")) {
            if (!param) {
                fprintf(stderr, "Missing argument (option: %s)\n", arg);
                CLEANUP_STRINGS;
                return EXIT_FAILURE;
            }
            config_file = param;
            argn++;
        }
        else if (streq(arg, "-e") || streq(arg, "--endpoint")) {
            if (!param) {
                fprintf(stderr, "Missing argument (option: %s)\n", arg);
                CLEANUP_STRINGS;
                return EXIT_FAILURE;
            }
            zstr_free(&endpoint);
            endpoint = strdup(param);
            argn++;
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", arg);
            CLEANUP_STRINGS;
            return EXIT_FAILURE;
        }
    }

    // Parse config file
    if (config_file) {
        log_debug("fty_info: loading configuration file '%s'", config_file);
        zconfig_t* config = zconfig_load(config_file);
        if (!config) {
            log_error("fty-info: failed to load config file %s: %m", config_file);
            CLEANUP_STRINGS;
            return EXIT_FAILURE;
        }

        if (streq(zconfig_get(config, "server/verbose", "0"), "1")) {
            verbose = true;
        }

        zstr_free(&linuxmetrics_interval);
        zstr_free(&actor_name);
        zstr_free(&endpoint);
        zstr_free(&path);

        linuxmetrics_interval = strdup(s_get(config, "server/check_interval", "30"));
        actor_name = strdup(s_get(config, "malamute/address", FTY_INFO_AGENT));
        endpoint   = strdup(s_get(config, "malamute/endpoint", MLM_ENDPOINT));
        path       = strdup(s_get(config, "parameters/path", DEFAULT_PATH));

        zconfig_destroy(&config);
    }

    ManageFtyLog::setInstanceFtylog(FTY_INFO_AGENT, FTY_COMMON_LOGGING_DEFAULT_CFG);
    if (verbose) {
        ManageFtyLog::getInstanceFtylog()->setVerboseMode();
    }

    // Defaults (sanity checks)
    if (!actor_name) { actor_name = strdup(FTY_INFO_AGENT); }
    if (!endpoint) { endpoint = strdup(MLM_ENDPOINT); }
    if (!path) { path = strdup(DEFAULT_PATH); }
    if (!linuxmetrics_interval) {
        asprintf(&linuxmetrics_interval, "%d", DEFAULT_LINUXMETRICS_INTERVAL_SEC);
    }

    // server configuration
    zactor_t* server = zactor_new(fty_info_server, actor_name);
    if (!server) {
        log_error("fty-info-server creation failed");
        CLEANUP_STRINGS;
        return EXIT_FAILURE;
    }
    zstr_sendx(server, "PATH", path, NULL);
    zstr_sendx(server, "CONFIG", HW_CAP_PATH, NULL);
    zstr_sendx(server, "CONNECT", endpoint, NULL);
    zstr_sendx(server, "CONSUMER", FTY_PROTO_STREAM_ASSETS, ".*", NULL);
    zstr_sendx(server, "PRODUCER", "ANNOUNCE", NULL);
    zstr_sendx(server, "ROOT_DIR", "/", NULL);
    zstr_sendx(server, "LINUXMETRICSINTERVAL", linuxmetrics_interval, NULL);

    // Run once actor to fill data about rackcontroller-0 asset
    zactor_t* rc0_runonce = zactor_new(fty_info_rc0_runonce, const_cast<char*>(RC0_RUNONCE_ACTOR));
    if (!rc0_runonce) {
        log_error("fty_info_rc0_runonce creation failed");
        zactor_destroy(&server);
        CLEANUP_STRINGS;
        return EXIT_FAILURE;
    }
    zstr_sendx(rc0_runonce, "CONNECT", endpoint, NULL);
    zstr_sendx(rc0_runonce, "CONSUMER", FTY_PROTO_STREAM_ASSETS, "device\\.rackcontroller.*", NULL);

    // timer to regularly update linux metrics
    zloop_t* timer_loop = zloop_new();
    if (!timer_loop) {
        log_error("timer_loop creation failed");
        zactor_destroy(&rc0_runonce);
        zactor_destroy(&server);
        CLEANUP_STRINGS;
        return EXIT_FAILURE;
    }
    zloop_timer(timer_loop, size_t(atoi(linuxmetrics_interval) * 1000), 0, s_linuxmetrics_event, server);
    zloop_start(timer_loop);

    // Main loop, accept any message back from server
    // copy from src/malamute.c under MPL license
    while (!zsys_interrupted) {
        char* msg = zstr_recv(server);
        if (!msg)
            break;
        log_trace("%s: recv msg '%s'", "fty-info", msg);
        zstr_free(&msg);
    }

    // Cleanup
    zloop_destroy(&timer_loop);
    zactor_destroy(&rc0_runonce);
    zactor_destroy(&server);
    CLEANUP_STRINGS;

    return EXIT_SUCCESS;
}
