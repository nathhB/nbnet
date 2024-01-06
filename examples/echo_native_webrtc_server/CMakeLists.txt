cmake_minimum_required(VERSION 3.1)

project(echo_native_webrtc_server)

option(CPP_COMPILE OFF)
option(ENABLE_TLS OFF)

# allow to compile as cpp
if (CPP_COMPILE)
  file(GLOB_RECURSE CFILES "${CMAKE_SOURCE_DIR}/*.c")
  SET_SOURCE_FILES_PROPERTIES(${CFILES} PROPERTIES LANGUAGE CXX)
  set (CMAKE_CXX_STANDARD 20)
endif (CPP_COMPILE)

unset(CPP_COMPILE)

add_executable(echo_native_webrtc_server server.c shared.c)

add_compile_options(-Wall -Wextra -Wpedantic)

if (ENABLE_TLS)
    message("Compile with TLS enabled")
    target_compile_definitions(echo_native_webrtc_server PUBLIC NBN_TLS)
endif (ENABLE_TLS)

unset(ENABLE_TLS)

target_compile_definitions(echo_native_webrtc_server PUBLIC NBN_DEBUG)

target_link_libraries(echo_native_webrtc_server ${LIBDATACHANNEL_LIBRARY_PATH} m)

target_include_directories(echo_native_webrtc_server PUBLIC "${LIBDATACHANNEL_INCLUDE_PATH}")
