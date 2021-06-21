cmake_minimum_required(VERSION 3.13)
##############################################################################################################

project(fty-info
  VERSION "1.0.0"
  DESCRIPTION "Agent that returns IPM2 system information"
  LANGUAGES CXX)

##############################################################################################################
find_package(fty-cmake PATHS ${CMAKE_BINARY_DIR}/fty-cmake)
##############################################################################################################

etn_target(static ${PROJECT_NAME}-lib
    SOURCES
        src/fty_info.h
        src/ftyinfo.cc
        src/ftyinfo.h
        src/fty_info_rc0_runonce.cc
        src/fty_info_rc0_runonce.h
        src/fty_info_server.cc
        src/fty_info_server.h
        src/linuxmetric.cc
        src/linuxmetric.h
        src/topologyresolver.cc
        src/topologyresolver.h
    USES
        czmq
        mlm
        cxxtools
        fty_proto
        fty_common
        fty_common_logging
        fty_shm
        stdc++fs
    PRIVATE
)

##############################################################################################################

etn_target(exe ${PROJECT_NAME}
    SOURCES
        src/fty_info.cc
    USES
        ${PROJECT_NAME}-lib
)

##############################################################################################################

# Agent config
etn_configure_file(
    resources/${PROJECT_NAME}.cfg.in
    TARGET      ${PROJECT_NAME}
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME}"
)

# systemd unit
etn_configure_file(
    resources/${PROJECT_NAME}.service.in
    TARGET      ${PROJECT_NAME}
    DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/systemd/system/"
)

##############################################################################################################

etn_test_target(${PROJECT_NAME}-lib
    CONFIGS
        tests/selftest-ro/*
    SOURCES
        tests/info_rc0_runonce.cpp
        tests/info_server.cpp
        tests/main.cpp
        tests/selftest-ro
        tests/topologyresolver.cpp
    PREPROCESSOR
        -DCATCH_CONFIG_FAST_COMPILE
    SUBDIR
        tests
)

##############################################################################################################
