/*  =========================================================================
    fty_info_rc0_runonce - Run once actor to update rackcontroller-0 (SN, ...)

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
    fty_info_rc0_runonce - Run once actor to update rackcontroller-0 (SN, ...)
@discuss
@end
*/

#include "fty_info_classes.h"

//  Structure of our class

struct _fty_info_rc0_runonce_t {
    char *name;
    char *endpoint;
    topologyresolver_t *resolver;
    ftyinfo_t *info;
    mlm_client_t *client;
};


//  --------------------------------------------------------------------------
//  Create a new fty_info_rc0_runonce

fty_info_rc0_runonce_t *
fty_info_rc0_runonce_new (char *name)
{
    fty_info_rc0_runonce_t *self = (fty_info_rc0_runonce_t *) zmalloc (sizeof (fty_info_rc0_runonce_t));
    assert (self);
    //  Initialize class properties here
    self->name=strdup(name);
    self->client = mlm_client_new ();
    self->resolver = topologyresolver_new (DEFAULT_RC_INAME);
    self->info = ftyinfo_new (self->resolver, DEFAULT_PATH);
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the fty_info_rc0_runonce

void
fty_info_rc0_runonce_destroy (fty_info_rc0_runonce_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        fty_info_rc0_runonce_t *self = *self_p;
        //  Free class properties here
        mlm_client_destroy (&(self->client));
        zstr_free(&(self->name));
        ftyinfo_destroy (&(self->info));
        topologyresolver_destroy (&(self->resolver));
        zstr_free(&(self->endpoint));
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Handle stream messages for this actor

int
handle_stream(fty_info_rc0_runonce_t *self, zmsg_t *msg)
{
    if (NULL == self || NULL == msg) {
        return -1;
    }
    if (!is_fty_proto (msg)){
        zmsg_destroy (&msg);
        return 0;
    }
    fty_proto_t *message = fty_proto_decode (&msg);
    if (NULL == message ) {
        log_error ("can't decode message with subject %s, ignoring", mlm_client_subject (self->client));
        return -1;
    }
    if (fty_proto_id (message) != FTY_PROTO_ASSET) {
        log_debug("Not FTY_PROTO_ASSET");
        fty_proto_destroy (&message);
        return 0;
    }

    const char *operation = fty_proto_operation (message);
    if (operation && !streq (operation, FTY_PROTO_ASSET_OP_UPDATE)) {
        log_debug("Not FTY_PROTO_ASSET_OP_UPDATE");
        fty_proto_destroy (&message);
        return 0;
    }

    const char *type = fty_proto_aux_string (message, "type", "");
    const char *subtype = fty_proto_aux_string (message, "subtype", "");
    if (!streq (type, "device") || !streq (subtype, "rackcontroller")) {
        log_debug ("Not device.rackcontroller");
        fty_proto_destroy (&message);
        return 0;
    }

    const char *iname = fty_proto_name(message);
    if (NULL == iname || !streq(iname, DEFAULT_RC_INAME)) {
        log_debug("Not %s", DEFAULT_RC_INAME);
        fty_proto_destroy (&message);
        return 0;
    }

    // just for rackcontroller-0
    int changeRW = 0;
    int changeRO = 0;

    fty_proto_t *messageRO = fty_proto_dup(message);
    zhash_t *ext = zhash_new ();
    zhash_autofree (ext);
    fty_proto_set_ext (messageRO, &ext);
    fty_proto_t *messageRW = fty_proto_dup(messageRO);

    const char *data = fty_proto_ext_string(message, "uuid", NULL);
    if (NULL == data) {
        if (NULL != self->info->uuid) {
            fty_proto_ext_insert(messageRO, "uuid", "%s", self->info->uuid);
            changeRO = 1;
        }
    }
    data = fty_proto_name(message);
    if ((NULL == data) || (0 == strcmp("", data))) {
        if (NULL != self->info->product) {
            fty_proto_set_name(messageRO, "%s", self->info->product);
            fty_proto_set_name(messageRW, "%s", self->info->product);
            changeRO = 1;
        }
    }
    data = fty_proto_ext_string(message, "manufacturer", NULL);
    if (NULL == data) {
        if (NULL != self->info->manufacturer) {
            fty_proto_ext_insert(messageRO, "manufacturer", "%s", self->info->manufacturer);
            changeRO = 1;
        }
    }
    data = fty_proto_ext_string(message, "serial_no", NULL);
    if (NULL == data) {
        if (NULL != self->info->serial) {
            fty_proto_ext_insert(messageRO, "serial_no", "%s", self->info->serial);
            changeRO = 1;
        }
    }
    data = fty_proto_ext_string(message, "contact_name", NULL);
    if (NULL == data) {
        if (NULL != self->info->contact) {
            fty_proto_ext_insert(messageRW, "contact_name", "%s", self->info->contact);
            changeRW = 1;
        }
    }
    data = fty_proto_ext_string(message, "description", NULL);
    if (NULL == data) {
        if ((NULL != self->info->hostname && 0 != strcmp("", self->info->hostname))
                && (NULL != self->info->description && 0 != strcmp("", self->info->description))
                && (NULL != self->info->installDate && 0 != strcmp("", self->info->installDate))) {
            fty_proto_ext_insert(messageRW, "description", "%s %s %s",
                    self->info->hostname, self->info->description, self->info->installDate);
            changeRW = 1;
        }
        else if ((NULL != self->info->hostname && 0 != strcmp("", self->info->hostname))
                && (NULL != self->info->description && 0 != strcmp("", self->info->description))) {
            fty_proto_ext_insert(messageRW, "description", "%s %s", self->info->hostname, self->info->description);
            changeRW = 1;
        }
        else if ((NULL != self->info->hostname && 0 != strcmp("", self->info->hostname))
                && (NULL != self->info->installDate && 0 != strcmp("", self->info->installDate))) {
            fty_proto_ext_insert(messageRW, "description", "%s %s", self->info->hostname, self->info->installDate);
            changeRW = 1;
        }
        else if ((NULL != self->info->description && 0 != strcmp("", self->info->description))
                && (NULL != self->info->installDate && 0 != strcmp("", self->info->installDate))) {
            fty_proto_ext_insert(messageRW, "description", "%s %s", self->info->description, self->info->installDate);
            changeRW = 1;
        }
        else if (NULL != self->info->hostname && 0 != strcmp("", self->info->hostname)) {
            fty_proto_ext_insert(messageRW, "description", "%s", self->info->hostname);
            changeRW = 1;
        }
        else if (NULL != self->info->description && 0 != strcmp("", self->info->description)) {
            fty_proto_ext_insert(messageRW, "description", "%s", self->info->description);
            changeRW = 1;
        }
        else if (NULL != self->info->installDate && 0 != strcmp("", self->info->installDate)) {
            fty_proto_ext_insert(messageRW, "description", "%s", self->info->installDate);
            changeRW = 1;
        }
    }

    data = fty_proto_ext_string(message, "ip.1", NULL);
    if (NULL == data) {
        struct ifaddrs *interfaces, *iface;
        char host[NI_MAXHOST];
        if (getifaddrs (&interfaces) != -1) {
            int counter = 0;
            for (iface = interfaces; iface != NULL; iface = iface->ifa_next) {
                if (iface->ifa_addr == NULL) continue;
                // here we support IPv4 only, only get first 3 addresses
                if (iface->ifa_addr->sa_family == AF_INET &&
                        getnameinfo(iface->ifa_addr,sizeof(struct sockaddr_in),
                                host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0) {
                    switch(counter) {
                        case 0:
                            fty_proto_ext_insert(messageRO, "ip.1", "%s", host);
                            break;
                        case 1:
                            fty_proto_ext_insert(messageRO, "ip.2", "%s", host);
                            break;
                        case 2:
                            fty_proto_ext_insert(messageRO, "ip.3", "%s", host);
                            break;
                    }
                    changeRO = 1;
                    ++counter;
                }
                if (counter == 3) {
                    break;
                }
            }
            freeifaddrs(interfaces);
        }
    }

    if (0 != changeRO) {
        fty_proto_t *messageDup = fty_proto_dup(messageRO);
        zmsg_t *msgDup = fty_proto_encode(&messageDup);
        zmsg_pushstrf (msgDup, "%s", "READONLY");
        int rv = mlm_client_sendto(self->client, "asset-agent", "ASSET_MANIPULATION", NULL, 10, &msgDup);
        if (rv == -1) {
            log_error("Failed to send ASSET_MANIPULATION message to asset-agent");
            fty_proto_destroy (&message);
            fty_proto_destroy (&messageRO);
            fty_proto_destroy (&messageRW);
            return -1;
        }
    }
    if(changeRW != 0) {
        fty_proto_t *messageDup = fty_proto_dup(messageRW);
        zmsg_t *msgDup = fty_proto_encode(&messageDup);
        zmsg_pushstrf (msgDup, "%s", "READWRITE");
        int rv = mlm_client_sendto(self->client, "asset-agent", "ASSET_MANIPULATION", NULL, 10, &msgDup);
        if (rv == -1) {
            log_error("Failed to send ASSET_MANIPULATION message to asset-agent");
            fty_proto_destroy (&message);
            fty_proto_destroy (&messageRO);
            fty_proto_destroy (&messageRW);
            return -1;
        }
    }

    fty_proto_destroy (&message);
    fty_proto_destroy (&messageRO);
    fty_proto_destroy (&messageRW);
    return 1;
}


//  --------------------------------------------------------------------------
//  Handle pipe messages for this actor

int
handle_pipe(fty_info_rc0_runonce_t *self, zmsg_t *message)
{
    if (NULL == self || NULL == message) {
        return 1;
    }
    char *command = zmsg_popstr (message);
    if (!command) {
        zmsg_destroy (&message);
        log_warning ("Empty command.");
        return 0;
    }
    if (streq(command, "$TERM")) {
        log_info ("Got $TERM");
        zmsg_destroy (&message);
        zstr_free (&command);
        return 1;
    }
    else
    if (streq(command, "CONNECT")) {
        char *endpoint = zmsg_popstr (message);

        if (endpoint) {
            self->endpoint = strdup(endpoint);
            log_debug ("fty-info-rc0-runonce: CONNECT: %s/%s", self->endpoint, self->name);
            int rv = mlm_client_connect (self->client, self->endpoint, 1000, self->name);
            if (rv == -1)
                log_error("mlm_client_connect failed\n");

        }
        zstr_free (&endpoint);
    }
    else
    if (streq (command, "CONSUMER")) {
        char* stream = zmsg_popstr (message);
        char* pattern = zmsg_popstr (message);
        int rv = mlm_client_set_consumer (self->client, stream, pattern);
        if (rv == -1)
            log_error ("%s: can't set consumer on stream '%s', '%s'",
                    self->name, stream, pattern);
        zstr_free (&pattern);
        zstr_free (&stream);
    }
    else
        log_error ("fty-info-rc0-runonce: Unknown actor command: %s.\n", command);

    zstr_free (&command);
    zmsg_destroy (&message);
    return 0;
}


//  --------------------------------------------------------------------------
//  Actor main thread

void fty_info_rc0_runonce (zsock_t *pipe, void *args)
{
    char *name = static_cast<char *>(args);
    if (!name) {
        log_error ("Address for fty-info-rc0-runonce actor is NULL");
        return;
    }
    fty_info_rc0_runonce_t *self = fty_info_rc0_runonce_new (name);

    zpoller_t *poller = zpoller_new (pipe, mlm_client_msgpipe (self->client), NULL);
    assert (poller);

    zsock_signal (pipe, 0);
    log_info ("fty-info-rc0-runonce: Started");

    while (!zsys_interrupted)
    {
        void *which = zpoller_wait (poller, TIMEOUT_MS);
        if (which == NULL) {
            if (zpoller_terminated (poller) || zsys_interrupted) {
                break;
            }
        }
        if (which == pipe) {
            log_debug ("which == pipe");
            if (0 != handle_pipe(self, zmsg_recv (pipe))) {
                log_debug("Broken pipe message");
                break;
            }
            else continue;
        }
        else
        if (which == mlm_client_msgpipe (self->client)) {
            zmsg_t *message = mlm_client_recv (self->client);
            if (!message)
                continue;
            const char *command = mlm_client_command (self->client);
            if (streq (command, "STREAM DELIVER")) {
                int rv = handle_stream (self, message);
                if (0 > rv) {
                    log_debug("Broken stream message");
                    break;
                }
                if (1 == rv) {
                    log_debug("RC-0 updated, finishing");
                    break;
                }
            }
            else {
                zmsg_destroy (&message);
            }
        }
    }

    zpoller_destroy (&poller);
    fty_info_rc0_runonce_destroy (&self);
}


//  --------------------------------------------------------------------------
//  Self test of this class

void
fty_info_rc0_runonce_test (bool verbose)
{
    printf (" * fty_info_rc0_runonce: ");

    //  @selftest
    //  Simple create/destroy test

    // Note: If your selftest reads SCMed fixture data, please keep it in
    // src/selftest-ro; if your test creates filesystem objects, please
    // do so under src/selftest-rw. They are defined below along with a
    // usecase for the variables (assert) to make compilers happy.
    const char *SELFTEST_DIR_RO = "src/selftest-ro";
    const char *SELFTEST_DIR_RW = "src/selftest-rw";
    assert (SELFTEST_DIR_RO);
    assert (SELFTEST_DIR_RW);
    // The following pattern is suggested for C selftest code:
    //    char *filename = NULL;
    //    filename = zsys_sprintf ("%s/%s", SELFTEST_DIR_RO, "mytemplate.file");
    //    assert (filename);
    //    ... use the filename for I/O ...
    //    zstr_free (&filename);
    // This way the same filename variable can be reused for many subtests.
    //
    // Uncomment these to use C++ strings in C++ selftest code:
    //std::string str_SELFTEST_DIR_RO = std::string(SELFTEST_DIR_RO);
    //std::string str_SELFTEST_DIR_RW = std::string(SELFTEST_DIR_RW);
    //assert ( (str_SELFTEST_DIR_RO != "") );
    //assert ( (str_SELFTEST_DIR_RW != "") );
    // NOTE that for "char*" context you need (str_SELFTEST_DIR_RO + "/myfilename").c_str()

    fty_info_rc0_runonce_t *self = fty_info_rc0_runonce_new (static_cast<char *>("myself"));
    assert (self);
    fty_info_rc0_runonce_destroy (&self);
    //  @end
    printf ("OK\n");
}
