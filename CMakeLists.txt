cmake_minimum_required(VERSION 3.13)
##############################################################################################################

project(fty-info
  VERSION "1.0.0"
  DESCRIPTION "Agent that returns IPM2 system information"
  LANGUAGES CXX)

##############################################################################################################
find_package(fty-cmake PATHS ${CMAKE_BINARY_DIR}/fty-cmake)
##############################################################################################################

etn_target(exe ${PROJECT_NAME}
    SOURCES
        src/topologyresolver.cc
        src/ftyinfo.cc
        src/linuxmetric.cc
        src/fty_info_server.cc
        src/fty_info_rc0_runonce.cc
        src/fty_info.cc              
        src/*.h
    USES_PRIVATE
        czmq
        cxxtools
        mlm
        fty_proto
        fty_common
        fty_common_logging
        fty_shm
)
 
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

if (BUILD_TESTING)

    etn_test(${PROJECT_NAME}-test
        SOURCES
            src/topologyresolver.cc
            src/ftyinfo.cc
            src/linuxmetric.cc
            src/fty_info_server.cc
            src/fty_info_rc0_runonce.cc
            src/test/main.cc
            src/fty_info_private_selftest.cc
            src/*.h
        PREPROCESSOR -DCATCH_CONFIG_FAST_COMPILE
        USES_PRIVATE
            czmq
            cxxtools
            mlm
            fty_proto
            fty_common
            fty_common_logging
            fty_shm
            Catch2::Catch2
            stdc++fs
            log4cplus
    )

    #copy selftest-ro for test
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/test/selftest-ro DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(MAKE_DIRECTORY  ${CMAKE_CURRENT_BINARY_DIR}/selftest-rw)

    #enable coverage
    etn_coverage(${PROJECT_NAME}-test)

    #copy selftest-ro for test coverage
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/test/selftest-ro DESTINATION ${CMAKE_BINARY_DIR})
    file(MAKE_DIRECTORY  ${CMAKE_BINARY_DIR}/selftest-rw)
endif()
