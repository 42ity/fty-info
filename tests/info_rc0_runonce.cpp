#include <catch2/catch.hpp>
#include "src/fty_info_rc0_runonce.h"

TEST_CASE("info rc0 runonce test")
{
    fty_info_rc0_runonce_t* self = fty_info_rc0_runonce_new(const_cast<char*>("myself"));
    assert(self);
    fty_info_rc0_runonce_destroy(&self);
}
