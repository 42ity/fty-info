/*  =========================================================================
    fty_info_server - 42ity info server

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
    fty_info_server - 42ity info server
@discuss
@end
*/
#include <string>
#include <unistd.h>
#include <bits/local_lim.h>
#include <cxxtools/jsondeserializer.h>
#include <istream>
#include <fstream>
#include <set>
#include <map>
#include <ifaddrs.h>

#include "fty_info_classes.h"

struct _fty_info_server_t {
    //  Declare class properties here
    char* name;
    char* endpoint;
    char* path;
    mlm_client_t *client;
    mlm_client_t *announce_client;
    mlm_client_t *info_client;
    bool verbose;
    bool first_announce;
    bool test;
    topologyresolver_t* resolver;
    int linuxmetrics_interval;
    std::string root_dir; //directory to be considered / - used for testing
    zhashx_t *history;
};

// this is kept for to handle with values set to ""
const char*
s_get (zconfig_t *config, const char* key, const char*dfl) {
    assert (config);
    char *ret = zconfig_get (config, key, dfl);
    if (!ret || streq (ret, ""))
        return dfl;
    return ret;
}

//  --------------------------------------------------------------------------
//  Free wrapper for zhashx destructor
static void history_destructor(void **item) {
    free(*item);
}

//  --------------------------------------------------------------------------
//  Create a new fty_info_server

fty_info_server_t  *
info_server_new (char *name)
{
    double *numerator_ptr = (double *)zmalloc(sizeof(double));
    double *denominator_ptr = (double *)zmalloc(sizeof(double));
    fty_info_server_t *self = new fty_info_server_t;
    assert (self);
    //  Initialize class properties here
    self->name=strdup(name);
    self->client = mlm_client_new ();
    self->announce_client = mlm_client_new ();
    self->info_client = mlm_client_new ();
    self->verbose=false;
    self->first_announce=true;
    self->test = false;
    self->history = zhashx_new();
    self->resolver = topologyresolver_new (DEFAULT_RC_INAME);
    zhashx_set_destructor(self->history, history_destructor);
    zhashx_insert(self->history, HIST_CPU_NUMERATOR, numerator_ptr);
    zhashx_insert(self->history, HIST_CPU_DENOMINATOR, denominator_ptr);
    return self;
}
//  --------------------------------------------------------------------------
//  Destroy the fty_info_server

void
info_server_destroy (fty_info_server_t  **self_p)
{
    assert (self_p);
    if (*self_p) {
        fty_info_server_t  *self = *self_p;
        //  Free class properties here
        mlm_client_destroy (&self->client);
        mlm_client_destroy (&self->announce_client);
        mlm_client_destroy (&self->info_client);
        zstr_free(&self->name);
        zstr_free(&self->endpoint);
        zstr_free(&self->path);
        topologyresolver_destroy (&self->resolver);
        zhashx_destroy(&self->history);
        //  Free object itself
        delete self;
        *self_p = NULL;
    }
}


//return IPC (uuid first 8 digits)
// the returned buffer should be freed
static
char *s_get_name(const char *name, const char *uuid)
{

    char *buffer = (char*)malloc(strlen(name)+12);
    char first_digit[9];
    if (uuid) {
        strncpy ( first_digit, uuid, 8 );
    } else {
        strncpy ( first_digit, DEFAULT_UUID, 8 );
    }
    first_digit[8]='\0';
    sprintf(buffer, "%s (%s)",name,first_digit);
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
static zmsg_t*
s_create_info (ftyinfo_t *info)
{
    zmsg_t *msg=zmsg_new();
    zmsg_addstr (msg, FTY_INFO_CMD);
    char *srv_name = s_get_name(SRV_NAME, ftyinfo_uuid(info));
    if (srv_name) {
        zmsg_addstr (msg, srv_name);
    } else {
        zmsg_addstr (msg, DEFAULT_UUID);
    }
    zmsg_addstr (msg, SRV_TYPE);
    zmsg_addstr (msg, SRV_STYPE);
    zmsg_addstr (msg, SRV_PORT);

    zhash_t *map = ftyinfo_infohash (info);
    zframe_t * frame_infos = zhash_pack(map);
    zmsg_append (msg, &frame_infos);

    zstr_free(&srv_name);
    zframe_destroy(&frame_infos);
    return msg;
}

//  --------------------------------------------------------------------------
//  publish INFO announcement on STREAM ANNOUNCE/ANNOUNCE-TEST
//  subject : CREATE/UPDATE
static void
s_publish_announce(fty_info_server_t  * self)
{

    if(!mlm_client_connected(self->announce_client))
        return;
    ftyinfo_t *info;
    if (!self->test) {
        info = ftyinfo_new (self->resolver,self->path);
    }
    else
        info = ftyinfo_test_new ();

    zmsg_t *msg = s_create_info (info);

    if (self->first_announce) {
        if (mlm_client_send (self->announce_client, "CREATE", &msg) != -1) {
            zsys_info("publish CREATE msg on ANNOUNCE STREAM");
            self->first_announce=false;
        }
        else
            zsys_error("cant publish CREATE msg on ANNOUNCE STREAM");
    } else {
        if (mlm_client_send (self->announce_client, "UPDATE", &msg) != -1)
            zsys_info("publish UPDATE msg on ANNOUNCE STREAM");
        else
            zsys_error("cant publish UPDATE msg on ANNOUNCE STREAM");
    }
    ftyinfo_destroy (&info);
}

//  --------------------------------------------------------------------------
//  publish Linux system info on STREAM METRICS
static void
s_publish_linuxmetrics (fty_info_server_t  * self)
{
    if(!mlm_client_connected(self->info_client))
        return;

    zlistx_t *info = linuxmetric_get_all
        (self->linuxmetrics_interval,
         self->history,
         self->root_dir,
         self->test);

    int ttl = 3 * self->linuxmetrics_interval; // in seconds
    char *rc_iname = topologyresolver_id (self->resolver);

    linuxmetric_t *metric = (linuxmetric_t *) zlistx_first (info);
    while (metric) {
        char *value = zsys_sprintf ("%lf", metric->value);
        zsys_debug ("Publishing metric %s, value %lf, unit %s", metric->type , metric->value, metric->unit);
        zmsg_t *msg = fty_proto_encode_metric (
                NULL,
                ::time (NULL),
                ttl,
                metric->type,
                rc_iname,
                value,
                metric->unit
                );
        char *subject = zsys_sprintf ("%s@%s", metric->type, rc_iname);
        if (mlm_client_send (self->info_client, subject, &msg) != -1) {
            zsys_info ("Metric %s published on METRICS stream", metric->type);
        }
        else {
            zsys_error ("Can't publish metric %s on METRICS stream", metric->type);
        }
        linuxmetric_destroy (&metric);
        metric = (linuxmetric_t *) zlistx_next (info);
        zstr_free (&subject);
        zstr_free (&value);
    }

    free(rc_iname);
    zlistx_destroy (&info);

}

//  --------------------------------------------------------------------------
//  process pipe message
//  return true means continue, false means TERM
bool static
s_handle_pipe(fty_info_server_t* self,zmsg_t *message)
{
    if (!message)
        return true;
    char *command = zmsg_popstr (message);
    if (!command) {
        zmsg_destroy (&message);
        zsys_warning ("Empty command.");
        return true;
    }
    if (streq(command, "$TERM")) {
        zsys_info ("Got $TERM");
        zmsg_destroy (&message);
        zstr_free (&command);
        return false;
    }
    else
    if (streq(command, "CONNECT")) {
        char *endpoint = zmsg_popstr (message);

        if (endpoint) {
            if (!self->test)
                topologyresolver_set_endpoint (self->resolver, endpoint);
            self->endpoint = strdup(endpoint);
            zsys_debug ("fty-info: CONNECT: %s/%s", self->endpoint, self->name);
            int rv = mlm_client_connect (self->client, self->endpoint, 1000, self->name);
            if (rv == -1)
                zsys_error("mlm_client_connect failed\n");

        }
        zstr_free (&endpoint);
    }
    else
    if (streq (command, "VERBOSE")) {
        self->verbose = true;
        zsys_debug ("fty-info: VERBOSE=true");
    }
    else
    if (streq (command, "PATH")) {
        char *path = zmsg_popstr (message);

        if (path) {
            self->path = strdup(path);
            zsys_debug ("fty-info: PATH: %s", self->path);
        }
        zstr_free (&path);
    }
    else
    if (streq (command, "CONSUMER")) {
        char* stream = zmsg_popstr (message);
        char* pattern = zmsg_popstr (message);
        int rv = mlm_client_set_consumer (self->client, stream, pattern);
        if (rv == -1)
            zsys_error ("%s: can't set consumer on stream '%s', '%s'",
                    self->name, stream, pattern);
        zstr_free (&pattern);
        zstr_free (&stream);
    }
    else
    if (streq (command, "PRODUCER")) {
        char* stream = zmsg_popstr (message);
        if (streq (stream, "ANNOUNCE-TEST") || streq (stream, "ANNOUNCE")) {
            self->test = streq(stream,"ANNOUNCE-TEST");
            if (!self->test) {
                zmsg_t *republish = zmsg_new ();
                int rv = mlm_client_sendto (self->client, FTY_ASSET_AGENT, "REPUBLISH", NULL, 5000, &republish);
                if ( rv != 0) {
                     zsys_error ("%s: cannot send REPUBLISH message", self->name);
                     zmsg_destroy (&republish);
                }
            }
            int rv = mlm_client_connect (self->announce_client, self->endpoint, 1000, "fty_info_announce");
            if (rv == -1)
                    zsys_error("fty_info_announce : mlm_client_connect failed\n");
            rv = mlm_client_set_producer (self->announce_client, stream);
            if (rv == -1)
                zsys_error ("%s: can't set producer on stream '%s'",
                        self->name, stream);
            else
                //do the first announce
                s_publish_announce(self);
        }
        else if (streq (stream, "METRICS-TEST") || streq (stream, FTY_PROTO_STREAM_METRICS)) {
            self->test = streq(stream,"METRICS-TEST");
            int rv = mlm_client_connect (self->info_client, self->endpoint, 1000, "fty_info_linuxmetrics");
            if (rv == -1)
                    zsys_error("fty_info_linuxmetrics : mlm_client_connect failed\n");
            rv = mlm_client_set_producer (self->info_client, stream);
            if (rv == -1)
                zsys_error ("%s: can't set producer on stream '%s'",
                        self->name, stream);
            else
                //publish the first metrics
                s_publish_linuxmetrics (self);
        }
        else {
            int rv = mlm_client_set_producer (self->client, stream);
            if (rv == -1)
                zsys_error ("%s: can't set producer on stream '%s'",
                        self->name, stream);
        }
        zstr_free (&stream);
    }
    else if (streq (command, "LINUXMETRICSINTERVAL")) {
        char *interval = zmsg_popstr (message);
        zsys_info ("Will be publishing metrics each %s seconds", interval);
        self->linuxmetrics_interval = (int) strtol (interval, NULL, 10);
        zstr_free (&interval);
    }
    else if (streq (command, "ROOT_DIR")) {
        char *root_dir = zmsg_popstr (message);
        zsys_info ("Will be using %s as root dir for finding out Linux metrics", root_dir);
        self->root_dir.assign (root_dir);
        zstr_free (&root_dir);
    }
    else if (streq (command, "TEST")) {
        self->test = true;
    }
    else if (streq (command, "ANNOUNCE")) {
        s_publish_announce (self);
    }
    else if (streq (command, "LINUXMETRICS")) {
        s_publish_linuxmetrics (self);
    }
    else
        zsys_error ("fty-info: Unknown actor command: %s.\n", command);

    zstr_free (&command);
    zmsg_destroy (&message);
    return true;
}

//  fty message freefn prototype

void fty_msg_free_fn(void *data)
{
    if (!data) return;
    fty_proto_t *msg = (fty_proto_t *)data;
    fty_proto_destroy (&msg);
}

//  --------------------------------------------------------------------------
//  process message from FTY_PROTO_ASSET stream
void static
s_handle_stream (fty_info_server_t* self, zmsg_t *message)
{
    if (!is_fty_proto (message)){
        zmsg_destroy (&message);
        return;
    }
    fty_proto_t *bmessage = fty_proto_decode (&message);
    if (!bmessage ) {
        zsys_error ("can't decode message with subject %s, ignoring", mlm_client_subject (self->client));
        zmsg_destroy (&message);
        return;
    }
    if (fty_proto_id (bmessage) != FTY_PROTO_ASSET) {
        fty_proto_destroy (&bmessage);
        zmsg_destroy (&message);
        return;

    }
    if(topologyresolver_asset (self->resolver, bmessage)) {
        s_publish_announce(self);
    }

    fty_proto_destroy (&bmessage);
    zmsg_destroy (&message);

}

//  --------------------------------------------------------------------------
//  process message from MAILBOX DELIVER INFO INFO/INFO-TEST
void static
s_handle_mailbox(fty_info_server_t* self,zmsg_t *message)
{
    char *command = zmsg_popstr (message);
    if (!command) {
        zmsg_destroy (&message);
        zsys_warning ("Empty subject.");
        return;
    }
    //we assume all request command are MAILBOX DELIVER, and subject="info"
    if (!streq(command, "INFO") && !streq(command, "INFO-TEST")) {
        char *zuuid = zmsg_popstr (message);
        zsys_warning ("fty-info: Received unexpected command '%s'", command);
        zmsg_t *reply = zmsg_new ();
        if (NULL != zuuid)
            zmsg_addstr(reply, zuuid);
        zmsg_addstr(reply, "ERROR");
        zmsg_addstr (reply, "unexpected command");
        mlm_client_sendto (self->client, mlm_client_sender (self->client), "info", NULL, 1000, &reply);
        zstr_free (&command);
        zmsg_destroy (&message);
        return;
    }
    else {
        zsys_debug ("fty-info:do '%s'", command);
        char *zuuid = zmsg_popstr (message);
        ftyinfo_t *info;
        if (streq(command, "INFO")) {
            info = ftyinfo_new (self->resolver,self->path);
        }
        if (streq(command, "INFO-TEST")) {
            info = ftyinfo_test_new ();
        }

        zmsg_t *reply = s_create_info (info);
        zmsg_pushstrf (reply, "%s", zuuid);
        mlm_client_sendto (self->client, mlm_client_sender (self->client), "info", NULL, 1000, &reply);
        zstr_free (&zuuid);
        ftyinfo_destroy (&info);
    }
    zstr_free (&command);
    zmsg_destroy (&message);

}
//  --------------------------------------------------------------------------
//  Create a new fty_info_server

void
fty_info_server (zsock_t *pipe, void *args)
{
    char *name = (char *)args;
    if (!name) {
        zsys_error ("Address for fty-info actor is NULL");
        return;
    }

    fty_info_server_t *self = info_server_new (name);
    zpoller_t *poller = zpoller_new (pipe, mlm_client_msgpipe (self->client), NULL);
    assert (poller);

    zsock_signal (pipe, 0);
    zsys_info ("fty-info: Started");


    while (!zsys_interrupted)
    {
        void *which = zpoller_wait (poller, TIMEOUT_MS);
        if (which == NULL) {
            if (zpoller_terminated (poller) || zsys_interrupted) {
                break;
            }
        }
        if (which == pipe) {
            if (self->verbose)
                zsys_debug ("which == pipe");
            if(!s_handle_pipe(self,zmsg_recv (pipe)))
                break;//TERM
            else continue;
        }
        else
        if (which == mlm_client_msgpipe (self->client)) {
            zmsg_t *message = mlm_client_recv (self->client);
            if (!message)
                continue;
            const char *command = mlm_client_command (self->client);
            if (streq (command, "STREAM DELIVER")) {
                s_handle_stream (self, message);
            }
            else
            if (streq (command, "MAILBOX DELIVER")) {
                s_handle_mailbox (self, message);
            }
        }
    }

    zpoller_destroy (&poller);
    info_server_destroy(&self);
}

//  --------------------------------------------------------------------------
//  Self test of this class

void
fty_info_server_test (bool verbose)
{
    printf (" * fty_info_server_test: ");

    //  @selftest

    static const char* endpoint = "inproc://fty-info-test";

    zactor_t *server = zactor_new (mlm_server, (void*) "Malamute");
    zstr_sendx (server, "BIND", endpoint, NULL);
    if (verbose)
        zstr_send (server, "VERBOSE");

    mlm_client_t *client = mlm_client_new ();
    mlm_client_connect (client, endpoint, 1000, "fty_info_server_test");


    zactor_t *info_server = zactor_new (fty_info_server, (void*) "fty-info");
    if (verbose)
        zstr_send (info_server, "VERBOSE");
    zstr_sendx (info_server, "TEST", NULL);
    zstr_sendx (info_server, "PATH", DEFAULT_PATH, NULL);
    zstr_sendx (info_server, "CONNECT", endpoint, NULL);
    zstr_sendx (info_server, "CONSUMER", FTY_PROTO_STREAM_ASSETS, ".*", NULL);
	zclock_sleep (1000);

    // Test #1: request INFO-TEST
    {
        zsys_debug ("fty-info-test:Test #1");
        zmsg_t *request = zmsg_new ();
        zmsg_addstr (request, "INFO-TEST");
        zuuid_t *zuuid = zuuid_new ();
        zmsg_addstrf (request, "%s", zuuid_str_canonical (zuuid));
        mlm_client_sendto (client, "fty-info", "info", NULL, 1000, &request);

        zmsg_t *recv = mlm_client_recv (client);

        assert (zmsg_size (recv) == 7);
        zsys_debug ("fty-info-test: zmsg_size = %d",zmsg_size (recv));
        char *zuuid_reply = zmsg_popstr (recv);
        assert (zuuid_reply && streq (zuuid_str_canonical(zuuid), zuuid_reply));

        char *cmd = zmsg_popstr (recv);
        assert (cmd && streq (cmd, FTY_INFO_CMD));
        char *srv_name = zmsg_popstr (recv);
        assert (srv_name && streq (srv_name,"IPC (ce7c523e)"));
        zsys_debug ("fty-info-test: srv name = '%s'", srv_name);
        char *srv_type = zmsg_popstr (recv);
        assert (srv_type && streq (srv_type,SRV_TYPE));
        zsys_debug ("fty-info-test: srv type = '%s'", srv_type);
        char *srv_stype = zmsg_popstr (recv);
        assert (srv_stype && streq (srv_stype,SRV_STYPE));
        zsys_debug ("fty-info-test: srv stype = '%s'", srv_stype);
        char *srv_port = zmsg_popstr (recv);
        assert (srv_port && streq (srv_port,SRV_PORT));
        zsys_debug ("fty-info-test: srv port = '%s'", srv_port);

        zframe_t *frame_infos = zmsg_next (recv);
        zhash_t *infos = zhash_unpack(frame_infos);

        char * uuid = (char *) zhash_lookup (infos, INFO_UUID);
        assert(uuid && streq (uuid,TST_UUID));
        zsys_debug ("fty-info-test: uuid = '%s'", uuid);
        char * hostname = (char *) zhash_lookup (infos, INFO_HOSTNAME);
        assert(hostname && streq (hostname, TST_HOSTNAME));
        zsys_debug ("fty-info-test: hostname = '%s'", hostname);
        char * name = (char *) zhash_lookup (infos, INFO_NAME);
        assert(name && streq (name, TST_NAME));
        zsys_debug ("fty-info-test: name = '%s'", name);
        char * name_uri = (char *) zhash_lookup (infos, INFO_NAME_URI);
        assert(name_uri && streq (name_uri, TST_NAME_URI));
        zsys_debug ("fty-info-test: name_uri = '%s'", name_uri);
        char * vendor = (char *) zhash_lookup (infos, INFO_VENDOR);
        assert(vendor && streq (vendor, TST_VENDOR));
        zsys_debug ("fty-info-test: vendor = '%s'", vendor);
        char * serial = (char *) zhash_lookup (infos, INFO_SERIAL);
        assert(serial && streq (serial, TST_SERIAL));
        zsys_debug ("fty-info-test: serial = '%s'", serial);
        char * product = (char *) zhash_lookup (infos, INFO_PRODUCT);
        assert(product && streq (product, TST_PRODUCT));
        zsys_debug ("fty-info-test: product = '%s'", product);
        char * location = (char *) zhash_lookup (infos, INFO_LOCATION);
        assert(location && streq (location, TST_LOCATION));
        zsys_debug ("fty-info-test: location = '%s'", location);
        char * parent_uri = (char *) zhash_lookup (infos, INFO_PARENT_URI);
        assert(parent_uri && streq (parent_uri, TST_PARENT_URI));
        zsys_debug ("fty-info-test: parent_uri = '%s'", parent_uri);
        char * version = (char *) zhash_lookup (infos, INFO_VERSION);
        assert(version && streq (version, TST_VERSION));
        zsys_debug ("fty-info-test: version = '%s'", version);
        char * rest_root = (char *) zhash_lookup (infos, INFO_REST_PATH);
        assert(rest_root && streq (rest_root, DEFAULT_PATH));
        zsys_debug ("fty-info-test: rest_path = '%s'", rest_root);
        zstr_free (&zuuid_reply);
        zstr_free (&cmd);
        zstr_free (&srv_name);
        zstr_free (&srv_type);
        zstr_free (&srv_stype);
        zstr_free (&srv_port);
        zhash_destroy(&infos);
        zmsg_destroy (&recv);
        zmsg_destroy (&request);
        zuuid_destroy (&zuuid);
        zsys_info ("fty-info-test:Test #1: OK");
    }
    // Test #2: request INFO
    {
        zsys_debug ("fty-info-test:Test #2");
        zmsg_t *request = zmsg_new ();
        zmsg_addstr (request, "INFO");
        zuuid_t *zuuid = zuuid_new ();
        zmsg_addstrf (request, "%s", zuuid_str_canonical (zuuid));
        mlm_client_sendto (client, "fty-info", "INFO", NULL, 1000, &request);

        zmsg_t *recv = mlm_client_recv (client);

        assert (zmsg_size (recv) == 7);
        char *zuuid_reply = zmsg_popstr (recv);
        assert (zuuid_reply && streq (zuuid_str_canonical(zuuid), zuuid_reply));

        char *cmd = zmsg_popstr (recv);
        assert (cmd && streq (cmd, FTY_INFO_CMD));

        char *srv_name  = zmsg_popstr (recv);
        char *srv_type  = zmsg_popstr (recv);
        char *srv_stype = zmsg_popstr (recv);
        char *srv_port  = zmsg_popstr (recv);

        zframe_t *frame_infos = zmsg_next (recv);
        zhash_t *infos = zhash_unpack(frame_infos);

        char *value = (char *) zhash_first (infos);   // first value
        while ( value != NULL )  {
            char *key = (char *) zhash_cursor (infos);   // key of this value
            zsys_debug ("fty-info-test: %s = %s",key,value);
            value     = (char *) zhash_next (infos);   // next value
        }
        zstr_free (&zuuid_reply);
        zstr_free (&cmd);
        zstr_free (&srv_name);
        zstr_free (&srv_type);
        zstr_free (&srv_stype);
        zstr_free (&srv_port);
        zhash_destroy(&infos);
        zmsg_destroy (&recv);
        zmsg_destroy (&request);
        zuuid_destroy (&zuuid);
        zsys_info ("fty-info-test:Test #2: OK");
    }
    mlm_client_t *asset_generator = mlm_client_new ();
    mlm_client_connect (asset_generator, endpoint, 1000, "fty_info_asset_generator");
    mlm_client_set_producer (asset_generator, FTY_PROTO_STREAM_ASSETS);
    // Test #3: process asset message - CREATE RC
    {
        zsys_debug ("fty-info-test:Test #3");
        const char *name = TST_NAME;
        const char *parent = TST_PARENT_INAME;
        zhash_t* aux = zhash_new ();
        zhash_t *ext = zhash_new ();
        zhash_autofree (aux);
        zhash_autofree (ext);
        zhash_update (aux, "type", (void *) "device");
	    zhash_update (aux, "subtype", (void *) "rackcontroller");
	    zhash_update (aux, "parent_name.1", (void *) parent);
        zhash_update (ext, "name", (void *) name);
        zhash_update (ext, "ip.1", (void *) "127.0.0.1");

        zmsg_t *msg = fty_proto_encode_asset (
                aux,
                TST_INAME,
                FTY_PROTO_ASSET_OP_CREATE,
                ext);

        int rv = mlm_client_send (asset_generator, "device.rackcontroller@rackcontroller-0", &msg);
        assert (rv == 0);
        zhash_destroy (&aux);
        zhash_destroy (&ext);

        zclock_sleep (1000);

        zmsg_t *request = zmsg_new ();
        zmsg_addstr (request, "INFO");
        zuuid_t *zuuid = zuuid_new ();
        zmsg_addstrf (request, "%s", zuuid_str_canonical (zuuid));
        mlm_client_sendto (client, "fty-info", "INFO", NULL, 1000, &request);

        zmsg_t *recv = mlm_client_recv (client);

        assert (zmsg_size (recv) == 7);
        char *zuuid_reply = zmsg_popstr (recv);
        assert (zuuid_reply && streq (zuuid_str_canonical(zuuid), zuuid_reply));
        char *cmd = zmsg_popstr (recv);
        assert (cmd && streq (cmd, FTY_INFO_CMD));
        char *srv_name  = zmsg_popstr (recv);
        char *srv_type  = zmsg_popstr (recv);
        char *srv_stype = zmsg_popstr (recv);
        char *srv_port  = zmsg_popstr (recv);

        zframe_t *frame_infos = zmsg_next (recv);
        zhash_t *infos = zhash_unpack(frame_infos);

        char *value = (char *) zhash_first (infos);   // first value
        while ( value != NULL )  {
            char *key = (char *) zhash_cursor (infos);   // key of this value
            zsys_debug ("fty-info-test: %s = %s",key,value);
            if (streq (key, INFO_NAME))
                assert (streq (value, TST_NAME));
            if (streq (key, INFO_NAME_URI))
                assert (streq (value, TST_NAME_URI));
            if (streq (key, INFO_PARENT_URI))
                assert (streq (value, TST_PARENT_URI));
            value     = (char *) zhash_next (infos);   // next value
        }
        zstr_free (&zuuid_reply);
        zstr_free (&cmd);
        zstr_free (&srv_name);
        zstr_free (&srv_type);
        zstr_free (&srv_stype);
        zstr_free (&srv_port);
        zhash_destroy(&infos);
        zmsg_destroy (&recv);
        zmsg_destroy (&request);
        zuuid_destroy (&zuuid);
        zsys_info ("fty-info-test:Test #3: OK");
    }
    //TEST #4: process asset message - UPDATE RC (change location)
    {
        zsys_debug ("fty-info-test:Test #4");
        zhash_t* aux = zhash_new ();
        zhash_t *ext = zhash_new ();
        zhash_autofree (aux);
        zhash_autofree (ext);
        const char *name = TST_NAME;
        const char *location = TST_PARENT2_INAME;
        zhash_update (aux, "type", (void *) "device");
        zhash_update (aux, "subtype", (void *) "rackcontroller");
        zhash_update (aux, "parent", (void *) location);
        zhash_update (ext, "name", (void *) name);
        zhash_update (ext, "ip.1", (void *) "127.0.0.1");

        zmsg_t *msg = fty_proto_encode_asset (
                aux,
                TST_INAME,
                FTY_PROTO_ASSET_OP_UPDATE,
                ext);

        int rv = mlm_client_send (asset_generator, "device.rackcontroller@rackcontroller-0", &msg);
        assert (rv == 0);
        zhash_destroy (&aux);
        zhash_destroy (&ext);

        zclock_sleep (1000);

        zmsg_t *request = zmsg_new ();
        zmsg_addstr (request, "INFO");
        zuuid_t *zuuid = zuuid_new ();
        zmsg_addstrf (request, "%s", zuuid_str_canonical (zuuid));
        mlm_client_sendto (client, "fty-info", "INFO", NULL, 1000, &request);

        zmsg_t *recv = mlm_client_recv (client);

        assert (zmsg_size (recv) == 7);
        char *zuuid_reply = zmsg_popstr (recv);
        assert (zuuid_reply && streq (zuuid_str_canonical(zuuid), zuuid_reply));
        char *cmd = zmsg_popstr (recv);
        assert (cmd && streq (cmd, FTY_INFO_CMD));
        char *srv_name  = zmsg_popstr (recv);
        char *srv_type  = zmsg_popstr (recv);
        char *srv_stype = zmsg_popstr (recv);
        char *srv_port  = zmsg_popstr (recv);
        zframe_t *frame_infos = zmsg_next (recv);
        zhash_t *infos = zhash_unpack(frame_infos);

        char *value = (char *) zhash_first (infos);   // first value
        while ( value != NULL )  {
            char *key = (char *) zhash_cursor (infos);   // key of this value
            zsys_debug ("fty-info-test: %s = %s",key,value);
            // if (streq (key, INFO_NAME))
            //     assert (streq (value, TST_NAME));
            // if (streq (key, INFO_NAME_URI))
            //     assert (streq (value, TST_NAME_URI));
            // if (streq (key, INFO_LOCATION_URI))
            //     assert (streq (value, TST_LOCATION2_URI));
            value     = (char *) zhash_next (infos);   // next value
        }
        zstr_free (&zuuid_reply);
        zstr_free (&cmd);
        zstr_free (&srv_name);
        zstr_free (&srv_type);
        zstr_free (&srv_stype);
        zstr_free (&srv_port);
        zhash_destroy(&infos);
        zmsg_destroy (&recv);
        zmsg_destroy (&request);
        zuuid_destroy (&zuuid);
        zsys_info ("fty-info-test:Test #4: OK");
    }
    //TEST #5: process asset message - do not process CREATE RC with IP address
    // which does not belong to us
    {
        zsys_debug ("fty-info-test:Test #5");
        zhash_t* aux = zhash_new ();
        zhash_t *ext = zhash_new ();
        zhash_autofree (aux);
        zhash_autofree (ext);
        const char *parent = TST_PARENT_INAME;
        zhash_update (aux, "type", (void *) "device");
        zhash_update (aux, "subtype", (void *) "rack controller");
        zhash_update (aux, "parent", (void *) parent);
        // use invalid IP address to make sure we don't have it
        zhash_update (ext, "ip.1", (void *) "300.3000.300.300");

        zmsg_t *msg = fty_proto_encode_asset (
                aux,
                TST_INAME,
                FTY_PROTO_ASSET_OP_CREATE,
                ext);

        int rv = mlm_client_send (asset_generator, "device.rack controller@rackcontroller-0", &msg);
        assert (rv == 0);
        zhash_destroy (&aux);
        zhash_destroy (&ext);

        zclock_sleep (1000);

        zmsg_t *request = zmsg_new ();
        zmsg_addstr (request, "INFO");
        zuuid_t *zuuid = zuuid_new ();
        zmsg_addstrf (request, "%s", zuuid_str_canonical (zuuid));
        mlm_client_sendto (client, "fty-info", "INFO", NULL, 1000, &request);

        zmsg_t *recv = mlm_client_recv (client);

        assert (zmsg_size (recv) == 7);
        char *zuuid_reply = zmsg_popstr (recv);
        assert (zuuid_reply && streq (zuuid_str_canonical(zuuid), zuuid_reply));
        char *cmd = zmsg_popstr (recv);
        assert (cmd && streq (cmd, FTY_INFO_CMD));
        char *srv_name  = zmsg_popstr (recv);
        char *srv_type  = zmsg_popstr (recv);
        char *srv_stype = zmsg_popstr (recv);
        char *srv_port  = zmsg_popstr (recv);

        zframe_t *frame_infos = zmsg_next (recv);
        zhash_t *infos = zhash_unpack(frame_infos);

        char *value = (char *) zhash_first (infos);   // first value
        while ( value != NULL )  {
            char *key = (char *) zhash_cursor (infos);   // key of this value
            zsys_debug ("fty-info-test: %s = %s",key,value);
            // if (streq (key, INFO_NAME))
            //     assert (streq (value, TST_NAME));
            // if (streq (key, INFO_NAME_URI))
            //     assert (streq (value, TST_NAME_URI));
            // if (streq (key, INFO_LOCATION_URI))
            //     assert (streq (value, TST_LOCATION2_URI));
            value     = (char *) zhash_next (infos);   // next value
        }
        zstr_free (&zuuid_reply);
        zstr_free (&cmd);
        zstr_free (&srv_name);
        zstr_free (&srv_type);
        zstr_free (&srv_stype);
        zstr_free (&srv_port);
        zhash_destroy(&infos);
        zmsg_destroy (&recv);
        zmsg_destroy (&request);
        zuuid_destroy (&zuuid);
        zsys_info ("fty-info-test:Test #5: OK");
    }
    // TEST #6 : test STREAM announce
    {
        zsys_debug ("fty-info-test:Test #6");
        int rv = mlm_client_set_consumer (client, "ANNOUNCE-TEST", ".*");
        assert(rv>=0);
        zstr_sendx (info_server, "PRODUCER", "ANNOUNCE-TEST", NULL);
        zmsg_t *recv = mlm_client_recv (client);
        assert(recv);
        const char *command = mlm_client_command (client);
        assert(streq (command, "STREAM DELIVER"));
        char* cmd = zmsg_popstr (recv);
        assert (cmd && streq (cmd, FTY_INFO_CMD));
        char *srv_name = zmsg_popstr (recv);
        assert (srv_name && streq (srv_name,"IPC (ce7c523e)"));
        zsys_debug ("fty-info-test: srv name = '%s'", srv_name);
        char *srv_type = zmsg_popstr (recv);
        assert (srv_type && streq (srv_type,SRV_TYPE));
        zsys_debug ("fty-info-test: srv type = '%s'", srv_type);
        char *srv_stype = zmsg_popstr (recv);
        assert (srv_stype && streq (srv_stype,SRV_STYPE));
        zsys_debug ("fty-info-test: srv stype = '%s'", srv_stype);
        char *srv_port = zmsg_popstr (recv);
        assert (srv_port && streq (srv_port,SRV_PORT));
        zsys_debug ("fty-info-test: srv port = '%s'", srv_port);

        zframe_t *frame_infos = zmsg_next (recv);
        zhash_t *infos = zhash_unpack(frame_infos);

        char * uuid = (char *) zhash_lookup (infos, INFO_UUID);
        assert(uuid && streq (uuid,TST_UUID));
        zsys_debug ("fty-info-test: uuid = '%s'", uuid);
        char * hostname = (char *) zhash_lookup (infos, INFO_HOSTNAME);
        assert(hostname && streq (hostname, TST_HOSTNAME));
        zsys_debug ("fty-info-test: hostname = '%s'", hostname);
        char * name = (char *) zhash_lookup (infos, INFO_NAME);
        assert(name && streq (name, TST_NAME));
        zsys_debug ("fty-info-test: name = '%s'", name);
        char * name_uri = (char *) zhash_lookup (infos, INFO_NAME_URI);
        assert(name_uri && streq (name_uri, TST_NAME_URI));
        zsys_debug ("fty-info-test: name_uri = '%s'", name_uri);
        char * vendor = (char *) zhash_lookup (infos, INFO_VENDOR);
        assert(vendor && streq (vendor, TST_VENDOR));
        zsys_debug ("fty-info-test: vendor = '%s'", vendor);
        char * serial = (char *) zhash_lookup (infos, INFO_SERIAL);
        assert(serial && streq (serial, TST_SERIAL));
        zsys_debug ("fty-info-test: serial = '%s'", serial);
        char * product = (char *) zhash_lookup (infos, INFO_PRODUCT);
        assert(product && streq (product, TST_PRODUCT));
        zsys_debug ("fty-info-test: product = '%s'", product);
        char * location = (char *) zhash_lookup (infos, INFO_LOCATION);
        assert(location && streq (location, TST_LOCATION));
        zsys_debug ("fty-info-test: location = '%s'", location);
        char * parent_uri = (char *) zhash_lookup (infos, INFO_PARENT_URI);
        assert(parent_uri && streq (parent_uri, TST_PARENT_URI));
        zsys_debug ("fty-info-test: parent_uri = '%s'", parent_uri);
        char * version = (char *) zhash_lookup (infos, INFO_VERSION);
        assert(version && streq (version, TST_VERSION));
        zsys_debug ("fty-info-test: version = '%s'", version);
        char * rest_root = (char *) zhash_lookup (infos, INFO_REST_PATH);
        assert(rest_root && streq (rest_root, DEFAULT_PATH));
        zsys_debug ("fty-info-test: rest_path = '%s'", rest_root);


        zstr_free (&srv_name);
        zstr_free (&srv_type);
        zstr_free (&srv_stype);
        zstr_free (&srv_port);
        zstr_free (&cmd);

        zhash_destroy(&infos);
        zmsg_destroy (&recv);
        zsys_info ("fty-info-test:Test #6: OK");
    }
    mlm_client_t *metric_reader = mlm_client_new ();
    mlm_client_connect (metric_reader, endpoint, 1000, "fty_info_metric_reader");
    mlm_client_set_consumer (metric_reader, "METRICS-TEST", ".*");
    // TEST #7 : test metrics - just types
    {
        zsys_debug ("fty-info-test:Test #7");

        // Note: If your selftest reads SCMed fixture data, please keep it in
        // src/selftest-ro; if your test creates filesystem objects, please
        // do so under src/selftest-rw. They are defined below along with a
        // usecase for the variables (assert) to make compilers happy.
        const char *SELFTEST_DIR_RO = "src/selftest-ro";
        const char *SELFTEST_DIR_RW = "src/selftest-rw";
        assert (SELFTEST_DIR_RO);
        assert (SELFTEST_DIR_RW);
        // Uncomment these to use C++ strings in C++ selftest code:
        std::string str_SELFTEST_DIR_RO = std::string(SELFTEST_DIR_RO);
        std::string str_SELFTEST_DIR_RW = std::string(SELFTEST_DIR_RW);
        assert ( (str_SELFTEST_DIR_RO != "") );
        assert ( (str_SELFTEST_DIR_RW != "") );
        // NOTE that for "char*" context you need (str_SELFTEST_DIR_RO + "/myfilename").c_str()

        std::string root_dir = str_SELFTEST_DIR_RO + "/data/";
        zstr_sendx (info_server, "ROOT_DIR", root_dir.c_str (), NULL);
        zstr_sendx (info_server, "LINUXMETRICSINTERVAL", "0", NULL);
        zstr_sendx (info_server, "PRODUCER", "METRICS-TEST", NULL);
        zclock_sleep (1000);

        zhashx_t *metrics = zhashx_new ();
        zhashx_set_destructor (metrics, (void (*)(void**)) fty_proto_destroy);
        // we have 12 non-network metrics
        for (int i = 0; i < 12; i++)
        {
            zmsg_t *recv = mlm_client_recv (metric_reader);
            assert(recv);
            const char *command = mlm_client_command (metric_reader);
            assert(streq (command, "STREAM DELIVER"));

            assert (fty_proto_is (recv));
            fty_proto_t *metric = fty_proto_decode (&recv);
            assert (fty_proto_id (metric) == FTY_PROTO_METRIC);
            const char* type = fty_proto_type (metric);
            zhashx_update (metrics, type, metric);
        }

        zhashx_t *interfaces = linuxmetric_list_interfaces (root_dir);
        const char *state = (const char *) zhashx_first (interfaces);
        while (state != NULL)  {
            const char *iface = (const char *) zhashx_cursor (interfaces);
            zsys_debug ("interface %s = %s", iface, state);

            if (streq (state, "up")) {
                // we have 3 network metrics: bandwidth, bytes, error_ratio
                // for both rx and tx
                for (int i = 0; i < 2*3; i++) {
                    zmsg_t *recv = mlm_client_recv (metric_reader);
                    assert(recv);
                    const char *command = mlm_client_command (metric_reader);
                    assert(streq (command, "STREAM DELIVER"));

                    assert (fty_proto_is (recv));
                    fty_proto_t *metric = fty_proto_decode (&recv);
                    assert (fty_proto_id (metric) == FTY_PROTO_METRIC);
                    const char* type = fty_proto_type (metric);
                    zhashx_update (metrics, type, metric);
                }
            }
            state = (const char *) zhashx_next (interfaces);
        }

        assert (zhashx_lookup (metrics, LINUXMETRIC_UPTIME));
        fty_proto_t *metric = (fty_proto_t *) zhashx_lookup (metrics, LINUXMETRIC_UPTIME);
        assert (1000000 == atoi (fty_proto_value (metric)));

        assert (zhashx_lookup (metrics, LINUXMETRIC_CPU_USAGE));
        metric = (fty_proto_t *) zhashx_lookup (metrics,LINUXMETRIC_CPU_USAGE);
        assert (50 == atoi (fty_proto_value (metric)));

        assert (zhashx_lookup (metrics, LINUXMETRIC_CPU_TEMPERATURE));
        metric = (fty_proto_t *) zhashx_lookup (metrics,LINUXMETRIC_CPU_TEMPERATURE);
        assert (50 == atoi (fty_proto_value (metric)));

        assert (zhashx_lookup (metrics, LINUXMETRIC_MEMORY_TOTAL));
        metric = (fty_proto_t *) zhashx_lookup (metrics,LINUXMETRIC_MEMORY_TOTAL);
        assert (4096 == atoi (fty_proto_value (metric)));

        assert (zhashx_lookup (metrics, LINUXMETRIC_MEMORY_USED));
        metric = (fty_proto_t *) zhashx_lookup (metrics,LINUXMETRIC_MEMORY_USED);
        assert (2048 == atoi (fty_proto_value (metric)));

        assert (zhashx_lookup (metrics, LINUXMETRIC_MEMORY_USAGE));
        metric = (fty_proto_t *) zhashx_lookup (metrics,LINUXMETRIC_MEMORY_USAGE);
        assert (50 == atoi (fty_proto_value (metric)));

        assert (zhashx_lookup (metrics, LINUXMETRIC_SDCARD_TOTAL));
        metric = (fty_proto_t *) zhashx_lookup (metrics,LINUXMETRIC_SDCARD_TOTAL);
        assert (10 == atoi (fty_proto_value (metric)));

        assert (zhashx_lookup (metrics, LINUXMETRIC_SDCARD_USED));
        metric = (fty_proto_t *) zhashx_lookup (metrics,LINUXMETRIC_SDCARD_USED);
        assert (1 == atoi (fty_proto_value (metric)));

        assert (zhashx_lookup (metrics, LINUXMETRIC_SDCARD_USAGE));
        metric = (fty_proto_t *) zhashx_lookup (metrics,LINUXMETRIC_SDCARD_USAGE);
        assert (10 == atoi (fty_proto_value (metric)));

        assert (zhashx_lookup (metrics, LINUXMETRIC_FLASH_TOTAL));
        metric = (fty_proto_t *) zhashx_lookup (metrics,LINUXMETRIC_FLASH_TOTAL);
        assert (10 == atoi (fty_proto_value (metric)));

        assert (zhashx_lookup (metrics, LINUXMETRIC_FLASH_USED));
        metric = (fty_proto_t *) zhashx_lookup (metrics,LINUXMETRIC_FLASH_USED);
        assert (5 == atoi (fty_proto_value (metric)));

        assert (zhashx_lookup (metrics, LINUXMETRIC_FLASH_USAGE));
        metric = (fty_proto_t *) zhashx_lookup (metrics,LINUXMETRIC_FLASH_USAGE);
        assert (50 == atoi (fty_proto_value (metric)));

        state = (const char *) zhashx_first (interfaces);
        while (state != NULL)  {
            const char *iface = (const char *) zhashx_cursor (interfaces);
            zsys_debug ("interface %s = %s", iface, state);

            if (streq (state, "up")) {
                char *rx_bandwidth = zsys_sprintf (BANDWIDTH_TEMPLATE, "rx", iface);
                assert (zhashx_lookup (metrics, rx_bandwidth));
                metric = (fty_proto_t *) zhashx_lookup (metrics, rx_bandwidth);
                assert (0 == atoi (fty_proto_value (metric)));
                zstr_free (&rx_bandwidth);

                char *rx_bytes = zsys_sprintf (BYTES_TEMPLATE, "rx", iface);
                assert (zhashx_lookup (metrics, rx_bytes));
                metric = (fty_proto_t *) zhashx_lookup (metrics, rx_bytes);
                assert (1000000 == atoi (fty_proto_value (metric)));
                zstr_free (&rx_bytes);

                char *rx_error_ratio = zsys_sprintf (ERROR_RATIO_TEMPLATE, "rx", iface);
                assert (zhashx_lookup (metrics, rx_error_ratio));
                metric = (fty_proto_t *) zhashx_lookup (metrics, rx_error_ratio);
                if (streq (iface, "LAN1"))
                    assert (1 == atoi (fty_proto_value (metric)));
                else
                    assert (0 == atoi (fty_proto_value (metric)));
                zstr_free (&rx_error_ratio);

                char *tx_bandwidth = zsys_sprintf (BANDWIDTH_TEMPLATE, "tx", iface);
                assert (zhashx_lookup (metrics, tx_bandwidth));
                metric = (fty_proto_t *) zhashx_lookup (metrics, tx_bandwidth);
                assert (0 == atoi (fty_proto_value (metric)));
                zstr_free (&tx_bandwidth);

                char *tx_bytes = zsys_sprintf (BYTES_TEMPLATE, "tx", iface);
                assert (zhashx_lookup (metrics, tx_bytes));
                metric = (fty_proto_t *) zhashx_lookup (metrics, tx_bytes);
                assert (1000000 == atoi (fty_proto_value (metric)));
                zstr_free (&tx_bytes);

                char *tx_error_ratio = zsys_sprintf (ERROR_RATIO_TEMPLATE, "tx", iface);
                assert (zhashx_lookup (metrics, tx_error_ratio));
                metric = (fty_proto_t *) zhashx_lookup (metrics, tx_error_ratio);
                if (streq (iface, "LAN1"))
                    assert (50 == atoi (fty_proto_value (metric)));
                else
                    assert (0 == atoi (fty_proto_value (metric)));
                zstr_free (&tx_error_ratio);
            }
            state = (const char *) zhashx_next (interfaces);
        }

        zhashx_destroy (&interfaces);
        zhashx_destroy (&metrics);
        zsys_info ("fty-info-test:Test #7: OK");
    }
    mlm_client_destroy (&metric_reader);
    mlm_client_destroy (&asset_generator);
    //  @end
    zactor_destroy (&info_server);
    mlm_client_destroy (&client);
    zactor_destroy (&server);
    zsys_info ("OK\n");
}
