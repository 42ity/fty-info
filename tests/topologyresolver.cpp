#include <catch2/catch.hpp>
#include "src/topologyresolver.h"
#include <malamute.h>

typedef enum
{
    DISCOVERING = 0,
    UPTODATE
} ResolverState;

struct _topologyresolver_t
{
    char*         iname;
    char*         topology;
    const char*   endpoint;
    ResolverState state;
    zhashx_t*     assets;
    mlm_client_t* client;
};

TEST_CASE("topologyresolver test")
{
    topologyresolver_t* resolver = topologyresolver_new("me");

    fty_proto_t* msg = fty_proto_new(FTY_PROTO_ASSET);
    fty_proto_set_name(msg, "grandparent");
    fty_proto_set_operation(msg, FTY_PROTO_ASSET_OP_CREATE);
    zhash_t* ext = zhash_new();
    zhash_autofree(ext);
    zhash_update(ext, "name", const_cast<char*>("my nice grandparent"));
    fty_proto_set_ext(msg, &ext);

    fty_proto_t* msg1 = fty_proto_new(FTY_PROTO_ASSET);
    fty_proto_set_name(msg1, "bogus");
    fty_proto_set_operation(msg1, FTY_PROTO_ASSET_OP_CREATE);
    ext = zhash_new();
    zhash_autofree(ext);
    zhash_update(ext, "name", const_cast<char*>("bogus asset"));
    fty_proto_set_ext(msg1, &ext);

    fty_proto_t* msg2 = fty_proto_new(FTY_PROTO_ASSET);
    fty_proto_set_name(msg2, "me");
    fty_proto_set_operation(msg2, FTY_PROTO_ASSET_OP_CREATE);
    ext = zhash_new();
    zhash_autofree(ext);
    zhash_update(ext, "name", const_cast<char*>("this is me"));
    fty_proto_set_ext(msg2, &ext);
    zhash_t* aux = zhash_new();
    zhash_autofree(aux);
    zhash_update(aux, "parent_name.1", const_cast<char*>("parent"));
    zhash_update(aux, "parent_name.2", const_cast<char*>("grandparent"));
    fty_proto_set_aux(msg2, &aux);

    fty_proto_t* msg3 = fty_proto_new(FTY_PROTO_ASSET);

    fty_proto_set_name(msg3, "parent");
    fty_proto_set_operation(msg3, FTY_PROTO_ASSET_OP_CREATE);
    ext = zhash_new();
    zhash_autofree(ext);
    zhash_update(ext, "name", const_cast<char*>("this is father"));
    fty_proto_set_ext(msg3, &ext);
    aux = zhash_new();
    zhash_autofree(aux);
    zhash_update(aux, "parent_name.1", const_cast<char*>("grandparent"));
    fty_proto_set_aux(msg3, &aux);

    char* res = topologyresolver_to_string(resolver);
    topologyresolver_asset(resolver, msg1);
    CHECK(nullptr == res);
    free(res);

    topologyresolver_asset(resolver, msg);
    CHECK(zhashx_size(resolver->assets) == 2);

    topologyresolver_asset(resolver, msg2);
    CHECK(zhashx_size(resolver->assets) == 3);
    res = topologyresolver_to_string(resolver);
    CHECK(nullptr == res);
    free(res);

    topologyresolver_asset(resolver, msg3);
    CHECK(zhashx_size(resolver->assets) == 3);
    res = topologyresolver_to_string(resolver, "->");
    CHECK(streq("my nice grandparent->this is father", res));
    free(res);

    fty_proto_t* msg4 = fty_proto_new(FTY_PROTO_ASSET);
    fty_proto_set_name(msg4, "me");
    fty_proto_set_operation(msg4, FTY_PROTO_ASSET_OP_UPDATE);
    ext = zhash_new();
    zhash_autofree(ext);
    zhash_update(ext, "name", const_cast<char*>("this is me"));
    fty_proto_set_ext(msg4, &ext);
    aux = zhash_new();
    zhash_autofree(aux);
    zhash_update(aux, "parent_name.1", const_cast<char*>("newparent"));
    zhash_update(aux, "parent_name.2", const_cast<char*>("grandparent"));
    fty_proto_set_aux(msg4, &aux);

    topologyresolver_asset(resolver, msg4);
    res = topologyresolver_to_string(resolver);
    CHECK(nullptr == res);
    free(res);

    fty_proto_t* msg5 = fty_proto_new(FTY_PROTO_ASSET);

    fty_proto_set_name(msg5, "newparent");
    fty_proto_set_operation(msg5, FTY_PROTO_ASSET_OP_CREATE);
    ext = zhash_new();
    zhash_autofree(ext);
    zhash_update(ext, "name", const_cast<char*>("this is new father"));
    fty_proto_set_ext(msg5, &ext);
    aux = zhash_new();
    zhash_autofree(aux);
    zhash_update(aux, "parent_name.1", const_cast<char*>("grandparent"));
    fty_proto_set_aux(msg5, &aux);

    topologyresolver_asset(resolver, msg5);
    res = topologyresolver_to_string(resolver, "->");
    CHECK(streq("my nice grandparent->this is new father", res));
    free(res);

    fty_proto_destroy(&msg5);
    fty_proto_destroy(&msg4);
    fty_proto_destroy(&msg3);
    fty_proto_destroy(&msg2);
    fty_proto_destroy(&msg1);
    fty_proto_destroy(&msg);

    topologyresolver_destroy(&resolver);
}
