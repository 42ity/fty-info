#include "src/fty_info.h"
#include "src/fty_info_server.h"
#include "src/ftyinfo.h"
#include "src/linuxmetric.h"
#include <catch2/catch.hpp>
#include <fty_shm.h>
#include <malamute.h>

TEST_CASE("info server test")
{
    // Note: If your selftest reads SCMed fixture data, please keep it in
    // src/test/selftest-ro; if your test creates filesystem objects, please
    // do so under src/test/selftest-rw. They are defined below along with a
    // usecase for the variables (assert) to make compilers happy.
    const char* SELFTEST_DIR_RO = "tests/selftest-ro";
    const char* SELFTEST_DIR_RW = ".";
    REQUIRE(SELFTEST_DIR_RO);
    REQUIRE(SELFTEST_DIR_RW);
    REQUIRE(fty_shm_set_test_dir(SELFTEST_DIR_RW) == 0);

    static const char* endpoint = "inproc://fty-info-test";

    zactor_t* server = zactor_new(mlm_server, const_cast<char*>("Malamute"));
    zstr_sendx(server, "BIND", endpoint, NULL);

    mlm_client_t* client = mlm_client_new();
    mlm_client_connect(client, endpoint, 1000, "fty_info_server_test");


    zactor_t* info_server = zactor_new(fty_info_server, const_cast<char*>("fty-info"));
    zstr_sendx(info_server, "TEST", NULL);
    zstr_sendx(info_server, "PATH", DEFAULT_PATH, NULL);
    zstr_sendx(info_server, "CONNECT", endpoint, NULL);
    zstr_sendx(info_server, "CONSUMER", FTY_PROTO_STREAM_ASSETS, ".*", NULL);
    zclock_sleep(1000);

    // Test #1: request INFO-TEST
    {
        zmsg_t* request = zmsg_new();
        zmsg_addstr(request, "INFO-TEST");
        zuuid_t* zuuid = zuuid_new();
        zmsg_addstrf(request, "%s", zuuid_str_canonical(zuuid));
        mlm_client_sendto(client, "fty-info", "info", NULL, 1000, &request);

        zmsg_t* recv = mlm_client_recv(client);

        REQUIRE(zmsg_size(recv) == 7);
        char* zuuid_reply = zmsg_popstr(recv);
        REQUIRE(zuuid_reply);
        CHECK(streq(zuuid_str_canonical(zuuid), zuuid_reply));

        char* cmd = zmsg_popstr(recv);
        REQUIRE(cmd);
        CHECK(streq(cmd, FTY_INFO_CMD));

        char* srv_name = zmsg_popstr(recv);
        REQUIRE(srv_name);
        CHECK(streq(srv_name, "IPC (ce7c523e)"));

        char* srv_type = zmsg_popstr(recv);
        REQUIRE(srv_type);
        CHECK(streq(srv_type, SRV_TYPE));

        char* srv_stype = zmsg_popstr(recv);
        REQUIRE(srv_stype);
        CHECK(streq(srv_stype, SRV_STYPE));

        char* srv_port = zmsg_popstr(recv);
        REQUIRE(srv_port);
        CHECK(streq(srv_port, SRV_PORT));

        zframe_t* frame_infos = zmsg_next(recv);
        zhash_t*  infos       = zhash_unpack(frame_infos);

        char* uuid = static_cast<char*>(zhash_lookup(infos, INFO_UUID));
        REQUIRE(uuid);
        CHECK(streq(uuid, TST_UUID));

        char* hostname = static_cast<char*>(zhash_lookup(infos, INFO_HOSTNAME));
        REQUIRE(hostname);
        CHECK(streq(hostname, TST_HOSTNAME));

        char* name = static_cast<char*>(zhash_lookup(infos, INFO_NAME));
        REQUIRE(name);
        CHECK(streq(name, TST_NAME));

        char* name_uri = static_cast<char*>(zhash_lookup(infos, INFO_NAME_URI));
        REQUIRE(name_uri);
        CHECK(streq(name_uri, TST_NAME_URI));

        char* vendor = static_cast<char*>(zhash_lookup(infos, INFO_VENDOR));
        REQUIRE(vendor);
        CHECK(streq(vendor, TST_VENDOR));

        char* serial = static_cast<char*>(zhash_lookup(infos, INFO_SERIAL));
        REQUIRE(serial);
        CHECK(streq(serial, TST_SERIAL));

        char* product = static_cast<char*>(zhash_lookup(infos, INFO_PRODUCT));
        REQUIRE(product);
        CHECK(streq(product, TST_PRODUCT));

        char* location = static_cast<char*>(zhash_lookup(infos, INFO_LOCATION));
        REQUIRE(location);
        CHECK(streq(location, TST_LOCATION));

        char* parent_uri = static_cast<char*>(zhash_lookup(infos, INFO_PARENT_URI));
        REQUIRE(parent_uri);
        CHECK(streq(parent_uri, TST_PARENT_URI));

        char* version = static_cast<char*>(zhash_lookup(infos, INFO_VERSION));
        REQUIRE(version);
        CHECK(streq(version, TST_VERSION));

        char* rest_root = static_cast<char*>(zhash_lookup(infos, INFO_REST_PATH));
        REQUIRE(rest_root);
        CHECK(streq(rest_root, DEFAULT_PATH));

        zstr_free(&zuuid_reply);
        zstr_free(&cmd);
        zstr_free(&srv_name);
        zstr_free(&srv_type);
        zstr_free(&srv_stype);
        zstr_free(&srv_port);
        zhash_destroy(&infos);
        zmsg_destroy(&recv);
        zmsg_destroy(&request);
        zuuid_destroy(&zuuid);
    }
    // Test #2: request INFO
    {
        zmsg_t* request = zmsg_new();
        zmsg_addstr(request, "INFO");
        zuuid_t* zuuid = zuuid_new();
        zmsg_addstrf(request, "%s", zuuid_str_canonical(zuuid));
        mlm_client_sendto(client, "fty-info", "INFO", NULL, 1000, &request);

        zmsg_t* recv = mlm_client_recv(client);

        REQUIRE(zmsg_size(recv) == 7);

        char* zuuid_reply = zmsg_popstr(recv);
        REQUIRE(zuuid_reply);
        CHECK(streq(zuuid_str_canonical(zuuid), zuuid_reply));

        char* cmd = zmsg_popstr(recv);
        REQUIRE(cmd);
        CHECK(streq(cmd, FTY_INFO_CMD));

        char* srv_name  = zmsg_popstr(recv);
        char* srv_type  = zmsg_popstr(recv);
        char* srv_stype = zmsg_popstr(recv);
        char* srv_port  = zmsg_popstr(recv);

        zframe_t* frame_infos = zmsg_next(recv);
        zhash_t*  infos       = zhash_unpack(frame_infos);

        char* value = static_cast<char*>(zhash_first(infos)); // first value
        while (value != NULL) {
            char* key = const_cast<char*>(zhash_cursor(infos)); // key of this value
            CHECK(key);
            value = static_cast<char*>(zhash_next(infos)); // next value
        }
        zstr_free(&zuuid_reply);
        zstr_free(&cmd);
        zstr_free(&srv_name);
        zstr_free(&srv_type);
        zstr_free(&srv_stype);
        zstr_free(&srv_port);
        zhash_destroy(&infos);
        zmsg_destroy(&recv);
        zmsg_destroy(&request);
        zuuid_destroy(&zuuid);
    }
    mlm_client_t* asset_generator = mlm_client_new();
    mlm_client_connect(asset_generator, endpoint, 1000, "fty_info_asset_generator");
    mlm_client_set_producer(asset_generator, FTY_PROTO_STREAM_ASSETS);
    // Test #3: process asset message - CREATE RC
    {
        const char* name   = TST_NAME;
        const char* parent = TST_PARENT_INAME;
        zhash_t*    aux    = zhash_new();
        zhash_t*    ext    = zhash_new();
        zhash_autofree(aux);
        zhash_autofree(ext);
        zhash_update(aux, "type", const_cast<char*>("device"));
        zhash_update(aux, "subtype", const_cast<char*>("rackcontroller"));
        zhash_update(aux, "parent_name.1", const_cast<char*>(parent));
        zhash_update(ext, "name", const_cast<char*>(name));
        zhash_update(ext, "ip.1", const_cast<char*>("127.0.0.1"));

        zmsg_t* msg = fty_proto_encode_asset(aux, TST_INAME, FTY_PROTO_ASSET_OP_CREATE, ext);

        int rv = mlm_client_send(asset_generator, "device.rackcontroller@rackcontroller-0", &msg);
        REQUIRE(rv == 0);
        zhash_destroy(&aux);
        zhash_destroy(&ext);

        zclock_sleep(1000);

        zmsg_t* request = zmsg_new();
        zmsg_addstr(request, "INFO");
        zuuid_t* zuuid = zuuid_new();
        zmsg_addstrf(request, "%s", zuuid_str_canonical(zuuid));
        mlm_client_sendto(client, "fty-info", "INFO", NULL, 1000, &request);

        zmsg_t* recv = mlm_client_recv(client);

        REQUIRE(zmsg_size(recv) == 7);
        char* zuuid_reply = zmsg_popstr(recv);
        REQUIRE(zuuid_reply);
        CHECK(streq(zuuid_str_canonical(zuuid), zuuid_reply));

        char* cmd = zmsg_popstr(recv);
        REQUIRE(cmd);
        CHECK(streq(cmd, FTY_INFO_CMD));

        char* srv_name  = zmsg_popstr(recv);
        char* srv_type  = zmsg_popstr(recv);
        char* srv_stype = zmsg_popstr(recv);
        char* srv_port  = zmsg_popstr(recv);

        zframe_t* frame_infos = zmsg_next(recv);
        zhash_t*  infos       = zhash_unpack(frame_infos);

        char* value = static_cast<char*>(zhash_first(infos)); // first value
        while (value != NULL) {
            char* key = const_cast<char*>(zhash_cursor(infos)); // key of this value
            if (streq(key, INFO_NAME))
                CHECK(streq(value, TST_NAME));
            if (streq(key, INFO_NAME_URI))
                CHECK(streq(value, TST_NAME_URI));
            if (streq(key, INFO_PARENT_URI))
                CHECK(streq(value, TST_PARENT_URI));
            value = static_cast<char*>(zhash_next(infos)); // next value
        }
        zstr_free(&zuuid_reply);
        zstr_free(&cmd);
        zstr_free(&srv_name);
        zstr_free(&srv_type);
        zstr_free(&srv_stype);
        zstr_free(&srv_port);
        zhash_destroy(&infos);
        zmsg_destroy(&recv);
        zmsg_destroy(&request);
        zuuid_destroy(&zuuid);
    }
    // TEST #4: process asset message - UPDATE RC (change location)
    {
        zhash_t* aux = zhash_new();
        zhash_t* ext = zhash_new();
        zhash_autofree(aux);
        zhash_autofree(ext);
        const char* name     = TST_NAME;
        const char* location = TST_PARENT2_INAME;
        zhash_update(aux, "type", const_cast<char*>("device"));
        zhash_update(aux, "subtype", const_cast<char*>("rackcontroller"));
        zhash_update(aux, "parent", const_cast<char*>(location));
        zhash_update(ext, "name", const_cast<char*>(name));
        zhash_update(ext, "ip.1", const_cast<char*>("127.0.0.1"));

        zmsg_t* msg = fty_proto_encode_asset(aux, TST_INAME, FTY_PROTO_ASSET_OP_UPDATE, ext);

        int rv = mlm_client_send(asset_generator, "device.rackcontroller@rackcontroller-0", &msg);
        REQUIRE(rv == 0);
        zhash_destroy(&aux);
        zhash_destroy(&ext);

        zclock_sleep(1000);

        zmsg_t* request = zmsg_new();
        zmsg_addstr(request, "INFO");
        zuuid_t* zuuid = zuuid_new();
        zmsg_addstrf(request, "%s", zuuid_str_canonical(zuuid));
        mlm_client_sendto(client, "fty-info", "INFO", NULL, 1000, &request);

        zmsg_t* recv = mlm_client_recv(client);
        REQUIRE(zmsg_size(recv) == 7);

        char* zuuid_reply = zmsg_popstr(recv);
        REQUIRE(zuuid_reply);
        CHECK(streq(zuuid_str_canonical(zuuid), zuuid_reply));

        char* cmd = zmsg_popstr(recv);
        REQUIRE(cmd);
        CHECK(streq(cmd, FTY_INFO_CMD));

        char*     srv_name    = zmsg_popstr(recv);
        char*     srv_type    = zmsg_popstr(recv);
        char*     srv_stype   = zmsg_popstr(recv);
        char*     srv_port    = zmsg_popstr(recv);
        zframe_t* frame_infos = zmsg_next(recv);
        zhash_t*  infos       = zhash_unpack(frame_infos);

        char* value = static_cast<char*>(zhash_first(infos)); // first value
        while (value != NULL) {
            char* key = const_cast<char*>(zhash_cursor(infos)); // key of this value
            CHECK(key);
            // if (streq (key, INFO_NAME))
            //     assert (streq (value, TST_NAME));
            // if (streq (key, INFO_NAME_URI))
            //     assert (streq (value, TST_NAME_URI));
            // if (streq (key, INFO_LOCATION_URI))
            //     assert (streq (value, TST_LOCATION2_URI));
            value = static_cast<char*>(zhash_next(infos)); // next value
        }
        zstr_free(&zuuid_reply);
        zstr_free(&cmd);
        zstr_free(&srv_name);
        zstr_free(&srv_type);
        zstr_free(&srv_stype);
        zstr_free(&srv_port);
        zhash_destroy(&infos);
        zmsg_destroy(&recv);
        zmsg_destroy(&request);
        zuuid_destroy(&zuuid);
    }
    // TEST #5: process asset message - do not process CREATE RC with IP address
    // which does not belong to us
    {
        zhash_t* aux = zhash_new();
        zhash_t* ext = zhash_new();
        zhash_autofree(aux);
        zhash_autofree(ext);
        const char* parent = TST_PARENT_INAME;
        zhash_update(aux, "type", const_cast<char*>("device"));
        zhash_update(aux, "subtype", const_cast<char*>("rack controller"));
        zhash_update(aux, "parent", const_cast<char*>(parent));
        // use invalid IP address to make sure we don't have it
        zhash_update(ext, "ip.1", const_cast<char*>("300.3000.300.300"));

        zmsg_t* msg = fty_proto_encode_asset(aux, TST_INAME, FTY_PROTO_ASSET_OP_CREATE, ext);

        int rv = mlm_client_send(asset_generator, "device.rack controller@rackcontroller-0", &msg);
        REQUIRE(rv == 0);
        zhash_destroy(&aux);
        zhash_destroy(&ext);

        zclock_sleep(1000);

        zmsg_t* request = zmsg_new();
        zmsg_addstr(request, "INFO");
        zuuid_t* zuuid = zuuid_new();
        zmsg_addstrf(request, "%s", zuuid_str_canonical(zuuid));
        mlm_client_sendto(client, "fty-info", "INFO", NULL, 1000, &request);

        zmsg_t* recv = mlm_client_recv(client);
        REQUIRE(zmsg_size(recv) == 7);

        char* zuuid_reply = zmsg_popstr(recv);
        REQUIRE(zuuid_reply);
        CHECK(streq(zuuid_str_canonical(zuuid), zuuid_reply));

        char* cmd = zmsg_popstr(recv);
        REQUIRE(cmd);
        CHECK(streq(cmd, FTY_INFO_CMD));

        char* srv_name  = zmsg_popstr(recv);
        char* srv_type  = zmsg_popstr(recv);
        char* srv_stype = zmsg_popstr(recv);
        char* srv_port  = zmsg_popstr(recv);

        zframe_t* frame_infos = zmsg_next(recv);
        zhash_t*  infos       = zhash_unpack(frame_infos);

        char* value = static_cast<char*>(zhash_first(infos)); // first value
        while (value != NULL) {
            char* key = const_cast<char*>(zhash_cursor(infos)); // key of this value
            CHECK(key);
            // if (streq (key, INFO_NAME))
            //     assert (streq (value, TST_NAME));
            // if (streq (key, INFO_NAME_URI))
            //     assert (streq (value, TST_NAME_URI));
            // if (streq (key, INFO_LOCATION_URI))
            //     assert (streq (value, TST_LOCATION2_URI));
            value = static_cast<char*>(zhash_next(infos)); // next value
        }
        zstr_free(&zuuid_reply);
        zstr_free(&cmd);
        zstr_free(&srv_name);
        zstr_free(&srv_type);
        zstr_free(&srv_stype);
        zstr_free(&srv_port);
        zhash_destroy(&infos);
        zmsg_destroy(&recv);
        zmsg_destroy(&request);
        zuuid_destroy(&zuuid);
    }
    // TEST #6 : test STREAM announce
    {
        int rv = mlm_client_set_consumer(client, "ANNOUNCE-TEST", ".*");
        REQUIRE(rv >= 0);
        zstr_sendx(info_server, "PRODUCER", "ANNOUNCE-TEST", NULL);
        zmsg_t* recv = mlm_client_recv(client);
        REQUIRE(recv);

        const char* command = mlm_client_command(client);
        REQUIRE(command);
        CHECK(streq(command, "STREAM DELIVER"));

        char* cmd = zmsg_popstr(recv);
        REQUIRE(cmd);
        CHECK(streq(cmd, FTY_INFO_CMD));

        char* srv_name = zmsg_popstr(recv);
        REQUIRE(srv_name);
        CHECK(streq(srv_name, "IPC (ce7c523e)"));

        char* srv_type = zmsg_popstr(recv);
        REQUIRE(srv_type);
        CHECK(streq(srv_type, SRV_TYPE));

        char* srv_stype = zmsg_popstr(recv);
        REQUIRE(srv_stype);
        CHECK(streq(srv_stype, SRV_STYPE));

        char* srv_port = zmsg_popstr(recv);
        REQUIRE(srv_port);
        CHECK(streq(srv_port, SRV_PORT));

        zframe_t* frame_infos = zmsg_next(recv);
        zhash_t*  infos       = zhash_unpack(frame_infos);

        char* uuid = static_cast<char*>(zhash_lookup(infos, INFO_UUID));
        REQUIRE(uuid);
        CHECK(streq(uuid, TST_UUID));

        char* hostname = static_cast<char*>(zhash_lookup(infos, INFO_HOSTNAME));
        REQUIRE(hostname);
        CHECK(streq(hostname, TST_HOSTNAME));

        char* name = static_cast<char*>(zhash_lookup(infos, INFO_NAME));
        REQUIRE(name);
        CHECK(streq(name, TST_NAME));

        char* name_uri = static_cast<char*>(zhash_lookup(infos, INFO_NAME_URI));
        REQUIRE(name_uri);
        CHECK(streq(name_uri, TST_NAME_URI));

        char* vendor = static_cast<char*>(zhash_lookup(infos, INFO_VENDOR));
        REQUIRE(vendor);
        CHECK(streq(vendor, TST_VENDOR));

        char* serial = static_cast<char*>(zhash_lookup(infos, INFO_SERIAL));
        REQUIRE(serial);
        CHECK(streq(serial, TST_SERIAL));

        char* product = static_cast<char*>(zhash_lookup(infos, INFO_PRODUCT));
        REQUIRE(product);
        CHECK(streq(product, TST_PRODUCT));

        char* location = static_cast<char*>(zhash_lookup(infos, INFO_LOCATION));
        REQUIRE(location);
        CHECK(streq(location, TST_LOCATION));

        char* parent_uri = static_cast<char*>(zhash_lookup(infos, INFO_PARENT_URI));
        REQUIRE(parent_uri);
        CHECK(streq(parent_uri, TST_PARENT_URI));

        char* version = static_cast<char*>(zhash_lookup(infos, INFO_VERSION));
        REQUIRE(version);
        CHECK(streq(version, TST_VERSION));

        char* rest_root = static_cast<char*>(zhash_lookup(infos, INFO_REST_PATH));
        REQUIRE(rest_root);
        CHECK(streq(rest_root, DEFAULT_PATH));

        zstr_free(&srv_name);
        zstr_free(&srv_type);
        zstr_free(&srv_stype);
        zstr_free(&srv_port);
        zstr_free(&cmd);

        zhash_destroy(&infos);
        zmsg_destroy(&recv);
    }
    // TEST #7 : test metrics - just types
    {
        std::string str_SELFTEST_DIR_RO = std::string(SELFTEST_DIR_RO);
        // NOTE that for "char*" context you need (str_SELFTEST_DIR_RO + "/myfilename").c_str()

        std::string root_dir = str_SELFTEST_DIR_RO + "/data/";
        zstr_sendx(info_server, "ROOT_DIR", root_dir.c_str(), NULL);
        zstr_sendx(info_server, "LINUXMETRICSINTERVAL", "30", NULL);
        zstr_sendx(info_server, "PRODUCER", "METRICS-TEST", NULL);

        zclock_sleep(1000);

        zhashx_t* metrics = zhashx_new();
        zhashx_set_destructor(metrics, reinterpret_cast<void (*)(void**)>(fty_proto_destroy));
        // we have 12 non-network metrics
        size_t      number_metrics = 12;
        zhashx_t*   interfaces     = linuxmetric_list_interfaces(root_dir);
        const char* state          = static_cast<const char*>(zhashx_first(interfaces));
        while (state != NULL) {
            const char* iface = static_cast<const char*>(zhashx_cursor(interfaces));
            CHECK(iface);

            if (streq(state, "up")) {
                // we have 3 network metrics: bandwidth, bytes, error_ratio
                // for both rx and tx
                number_metrics += (2 * 3);
            }
            state = static_cast<const char*>(zhashx_next(interfaces));
        }
        {
            zclock_sleep(1000);
            fty::shm::shmMetrics results;
            fty::shm::read_metrics(".*", ".*", results);
            assert(results.size() == number_metrics);
            for (auto& metric : results) {
                assert(fty_proto_id(metric) == FTY_PROTO_METRIC);
                const char* type = fty_proto_type(metric);
                zhashx_update(metrics, type, fty_proto_dup(metric));
            }
        }

        CHECK(zhashx_lookup(metrics, LINUXMETRIC_UPTIME));
        fty_proto_t* metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, LINUXMETRIC_UPTIME));
        CHECK(1000000 == atoi(fty_proto_value(metric)));

        CHECK(zhashx_lookup(metrics, LINUXMETRIC_CPU_USAGE));
        metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, LINUXMETRIC_CPU_USAGE));
        CHECK(50 == atoi(fty_proto_value(metric)));

        CHECK(zhashx_lookup(metrics, LINUXMETRIC_CPU_TEMPERATURE));
        metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, LINUXMETRIC_CPU_TEMPERATURE));
        CHECK(50 == atoi(fty_proto_value(metric)));

        CHECK(zhashx_lookup(metrics, LINUXMETRIC_MEMORY_TOTAL));
        metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, LINUXMETRIC_MEMORY_TOTAL));
        CHECK(4096 == atoi(fty_proto_value(metric)));

        CHECK(zhashx_lookup(metrics, LINUXMETRIC_MEMORY_USED));
        metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, LINUXMETRIC_MEMORY_USED));
        CHECK(1024 == atoi(fty_proto_value(metric)));

        CHECK(zhashx_lookup(metrics, LINUXMETRIC_MEMORY_USAGE));
        metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, LINUXMETRIC_MEMORY_USAGE));
        CHECK(25 == atoi(fty_proto_value(metric)));

        CHECK(zhashx_lookup(metrics, LINUXMETRIC_DATA0_TOTAL));
        metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, LINUXMETRIC_DATA0_TOTAL));
        CHECK(10 == atoi(fty_proto_value(metric)));

        CHECK(zhashx_lookup(metrics, LINUXMETRIC_DATA0_USED));
        metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, LINUXMETRIC_DATA0_USED));
        CHECK(1 == atoi(fty_proto_value(metric)));

        CHECK(zhashx_lookup(metrics, LINUXMETRIC_DATA0_USAGE));
        metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, LINUXMETRIC_DATA0_USAGE));
        CHECK(10 == atoi(fty_proto_value(metric)));

        CHECK(zhashx_lookup(metrics, LINUXMETRIC_SYSTEM_TOTAL));
        metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, LINUXMETRIC_SYSTEM_TOTAL));
        CHECK(10 == atoi(fty_proto_value(metric)));

        CHECK(zhashx_lookup(metrics, LINUXMETRIC_SYSTEM_USED));
        metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, LINUXMETRIC_SYSTEM_USED));
        CHECK(5 == atoi(fty_proto_value(metric)));

        CHECK(zhashx_lookup(metrics, LINUXMETRIC_SYSTEM_USAGE));
        metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, LINUXMETRIC_SYSTEM_USAGE));
        CHECK(50 == atoi(fty_proto_value(metric)));

        state = static_cast<const char*>(zhashx_first(interfaces));
        while (state != NULL) {
            const char* iface = static_cast<const char*>(zhashx_cursor(interfaces));

            if (streq(state, "up")) {
                char* rx_bandwidth = zsys_sprintf(BANDWIDTH_TEMPLATE, "rx", iface);
                CHECK(zhashx_lookup(metrics, rx_bandwidth));
                metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, rx_bandwidth));
                CHECK(33333 == atoi(fty_proto_value(metric)));
                zstr_free(&rx_bandwidth);

                char* rx_bytes = zsys_sprintf(BYTES_TEMPLATE, "rx", iface);
                CHECK(zhashx_lookup(metrics, rx_bytes));
                metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, rx_bytes));
                CHECK(1000000 == atoi(fty_proto_value(metric)));
                zstr_free(&rx_bytes);

                char* rx_error_ratio = zsys_sprintf(ERROR_RATIO_TEMPLATE, "rx", iface);
                CHECK(zhashx_lookup(metrics, rx_error_ratio));
                metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, rx_error_ratio));
                if (streq(iface, "LAN1"))
                    CHECK(1 == atoi(fty_proto_value(metric)));
                else
                    CHECK(0 == atoi(fty_proto_value(metric)));
                zstr_free(&rx_error_ratio);

                char* tx_bandwidth = zsys_sprintf(BANDWIDTH_TEMPLATE, "tx", iface);
                CHECK(zhashx_lookup(metrics, tx_bandwidth));
                metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, tx_bandwidth));
                CHECK(33333 == atoi(fty_proto_value(metric)));
                zstr_free(&tx_bandwidth);

                char* tx_bytes = zsys_sprintf(BYTES_TEMPLATE, "tx", iface);
                CHECK(zhashx_lookup(metrics, tx_bytes));
                metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, tx_bytes));
                CHECK(1000000 == atoi(fty_proto_value(metric)));
                zstr_free(&tx_bytes);

                char* tx_error_ratio = zsys_sprintf(ERROR_RATIO_TEMPLATE, "tx", iface);
                CHECK(zhashx_lookup(metrics, tx_error_ratio));
                metric = static_cast<fty_proto_t*>(zhashx_lookup(metrics, tx_error_ratio));
                if (streq(iface, "LAN1"))
                    CHECK(50 == atoi(fty_proto_value(metric)));
                else
                    CHECK(0 == atoi(fty_proto_value(metric)));
                zstr_free(&tx_error_ratio);
            }
            state = static_cast<const char*>(zhashx_next(interfaces));
        }

        zhashx_destroy(&interfaces);
        zhashx_destroy(&metrics);
    }
    {
        // TEST #8: hw capability info
        zstr_sendx(info_server, "CONFIG", "./selftest-ro/data/hw_cap", NULL);

        zmsg_t* hw_req = zmsg_new();
        zmsg_addstr(hw_req, "HW_CAP");
        zmsg_addstr(hw_req, "uuid1234");
        zmsg_addstr(hw_req, "gpo");
        zclock_sleep(1000);

        mlm_client_sendto(client, "fty-info", "info", NULL, 1000, &hw_req);

        zmsg_t* recv = mlm_client_recv(client);
        REQUIRE(recv);

        char* val = zmsg_popstr(recv);
        CHECK(streq(val, "uuid1234"));
        zstr_free(&val);
        val = zmsg_popstr(recv);
        CHECK(streq(val, "OK"));
        zstr_free(&val);
        val = zmsg_popstr(recv);
        CHECK(streq(val, "gpo"));
        zstr_free(&val);
        val = zmsg_popstr(recv);
        CHECK(streq(val, "5"));
        zstr_free(&val);
        val = zmsg_popstr(recv);
        CHECK(streq(val, "488"));
        zstr_free(&val);
        val = zmsg_popstr(recv);
        CHECK(streq(val, "20"));
        zstr_free(&val);
        val = zmsg_popstr(recv);
        CHECK(streq(val, "p4"));
        zstr_free(&val);
        val = zmsg_popstr(recv);
        CHECK(streq(val, "502"));
        zstr_free(&val);

        zmsg_destroy(&recv);
    }
    {
        // TEST #9: hw capability info
        zmsg_t* hw_req = zmsg_new();
        zmsg_addstr(hw_req, "HW_CAP");
        zmsg_addstr(hw_req, "uuid1234");
        zmsg_addstr(hw_req, "incorrect type");
        zclock_sleep(1000);

        mlm_client_sendto(client, "fty-info", "info", NULL, 1000, &hw_req);

        zmsg_t* recv = mlm_client_recv(client);
        REQUIRE(recv);

        char* val = zmsg_popstr(recv);
        CHECK(streq(val, "uuid1234"));
        zstr_free(&val);
        val = zmsg_popstr(recv);
        CHECK(streq(val, "ERROR"));
        zstr_free(&val);
        val = zmsg_popstr(recv);
        CHECK(streq(val, "unsupported type"));
        zstr_free(&val);

        zmsg_destroy(&recv);
    }
    {
        // TEST #10: hw capability: type
        zmsg_t* hw_req = zmsg_new();
        zmsg_addstr(hw_req, "HW_CAP");
        zmsg_addstr(hw_req, "uuid1235");
        zmsg_addstr(hw_req, "type");

        mlm_client_sendto(client, "fty-info", "info", NULL, 1000, &hw_req);

        zmsg_t* recv = mlm_client_recv(client);
        REQUIRE(recv);

        char* val = zmsg_popstr(recv);
        CHECK(streq(val, "uuid1235"));
        zstr_free(&val);
        val = zmsg_popstr(recv);
        CHECK(streq(val, "OK"));
        zstr_free(&val);
        val = zmsg_popstr(recv);
        CHECK(streq(val, "type"));
        zstr_free(&val);
        val = zmsg_popstr(recv);
        CHECK(streq(val, "ipc"));
        zstr_free(&val);
        CHECK(zmsg_popstr(recv) == NULL);

        zmsg_destroy(&recv);
    }

    mlm_client_destroy(&asset_generator);
    //  @end

    mlm_client_destroy(&client);
    zactor_destroy(&info_server);
    zactor_destroy(&server);
    fty_shm_delete_test_dir();
}
