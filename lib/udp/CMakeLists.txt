cmake_minimum_required(VERSION 3.6)
project(nbnet_udp)

set(SOURCES ../../nbnet.c)
add_library(nbnet_udp SHARED ${SOURCES})
target_compile_definitions(nbnet_udp PUBLIC NBN_UDP)

if (CMAKE_BUILD_TYPE MATCHES "Debug")
    message("Compiling in Debug mode")

    target_compile_definitions(nbnet_udp PUBLIC NBN_DEBUG)
endif()
