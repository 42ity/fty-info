/*  =========================================================================
    fty_info - Agent which returns rack controller information

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

/*
@header
    fty_info - Agent which returns rack controller information
@discuss
@end
*/

#include "fty_info_classes.h"

static int
s_linuxmetrics_event (zloop_t *loop, int timer_id, void *output)
{
    zstr_send (output, "LINUXMETRICS");
    return 0;
}

static int
s_announce_event (zloop_t *loop, int timer_id, void *output)
{
    zstr_send (output, "ANNOUNCE");
    return 0;
}

void
usage(){
    puts   ("fty-info [options] ...");
    puts   ("  -v|--verbose        verbose test output");
    puts   ("  -h|--help           this information");
    puts   ("  -c|--config         path to config file\n");
    puts   ("  -e|--endpoint       malamute endpoint [ipc://@/malamute]");
    printf ("  -a|--announce       how often (in seconds) publish information on stream [%d]\n", DEFAULT_ANNOUNCE_INTERVAL_SEC);

}

int main (int argc, char *argv [])
{
    int announcing = DEFAULT_ANNOUNCE_INTERVAL_SEC;
    int linuxmetrics_interval = DEFAULT_LINUXMETRICS_INTERVAL_SEC;
    const char *str_linuxmetrics_interval = NULL;
    char *config_file = NULL;
    zconfig_t *config = NULL;
    char* actor_name = NULL;
    char* endpoint = NULL;
    bool verbose = false;
    int argn;

    // Parse command line
    for (argn = 1; argn < argc; argn++) {
        char *param = NULL;
        if (argn < argc - 1) param = argv [argn+1];

        if (streq (argv [argn], "--help")
        ||  streq (argv [argn], "-h")) {
            usage();
            return 0;
        }
        else if (streq (argv [argn], "--verbose") ||  streq (argv [argn], "-v")) {
            verbose = true;
        }
        else if (streq (argv [argn], "--announce") ||  streq (argv [argn], "-a")) {
            ++argn;
            if (argn < argc) {
                announcing = atoi (argv [argn]);
            }
        }
        else if (streq (argv [argn], "--config") || streq (argv [argn], "-c")) {
            if (param) config_file = param;
            ++argn;
        }
        else if (streq (argv [argn], "--endpoint") || streq (argv [argn], "-e")) {
            if (param) endpoint = strdup(param);
            ++argn;
        }
        else {
            // FIXME: as per the systemd service file, the config file
            // is provided as the default arg without '-c'!
            // So, should we consider this?
            printf ("Unknown option: %s\n", argv [argn]);
            return 1;
        }
    }

    // Parse config file
    if(config_file) {
        my_zsys_debug (verbose, "fty_info: loading configuration file '%s'", config_file);
        config = zconfig_load (config_file);
        if (!config) {
            zsys_error ("Failed to load config file %s: %m", config_file);
            exit (EXIT_FAILURE);
        }
        // VERBOSE
        if (streq (zconfig_get (config, "server/verbose", "false"), "true")) {
            verbose = true;
        }

        // Announcement interval (in seconds)
        const char *str_announcing = s_get (config, "server/announce", "60");
        if (str_announcing) {
            announcing = atoi(str_announcing);
        }

	 // Linux metrics publishing interval (in seconds)
        str_linuxmetrics_interval = s_get (config, "server/check_interval", "30");
        if (str_linuxmetrics_interval) {
            linuxmetrics_interval = atoi (str_linuxmetrics_interval);
        }

        if (endpoint) zstr_free(&endpoint);
        endpoint = strdup(s_get (config, "malamute/endpoint", NULL));
        actor_name = strdup(s_get (config, "malamute/address", NULL));
    }
    if (actor_name == NULL)
        actor_name = strdup(FTY_INFO_AGENT);
    if (endpoint == NULL)
        endpoint = strdup("ipc://@/malamute");

    // Check env. variables
    if (getenv ("BIOS_LOG_LEVEL") && streq (getenv ("BIOS_LOG_LEVEL"), "LOG_DEBUG"))
        verbose = true;

    zactor_t *server = zactor_new (fty_info_server, (void*) actor_name);

    //  Insert main code here
    if (verbose) {
        zstr_sendx (server, "VERBOSE", NULL);
        zsys_info ("fty_info - Agent which returns rack controller information");
    }

    zstr_sendx (server, "CONNECT", endpoint, actor_name, NULL);
    zstr_sendx (server, "CONSUMER", FTY_PROTO_STREAM_ASSETS, ".*", NULL);
    zstr_sendx (server, "PRODUCER", "ANNOUNCE", NULL);
    zstr_sendx (server, "PRODUCER", FTY_PROTO_STREAM_METRICS, NULL);
    zstr_sendx (server, "LINUXMETRICSINTERVAL", str_linuxmetrics_interval, NULL);

    zloop_t *timer_loop = zloop_new();
    zloop_timer (timer_loop, announcing * 1000, 0, s_announce_event, server);
    zloop_timer (timer_loop, linuxmetrics_interval * 1000, 0, s_linuxmetrics_event, server);
    zloop_start (timer_loop);

    // Cleanup
    zloop_destroy (&timer_loop);
    zactor_destroy (&server);
    zstr_free(&actor_name);
    zstr_free(&endpoint);
    zconfig_destroy (&config);

    return 0;
}
