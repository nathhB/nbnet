cmake_minimum_required(VERSION 3.1)

project(echo)

option(CPP_COMPILE OFF)
option(ENABLE_TLS OFF)
option(WEBRTC_NATIVE OFF)

# allow to compile as cpp
if (CPP_COMPILE)
  file(GLOB_RECURSE CFILES "${CMAKE_SOURCE_DIR}/*.c")
  SET_SOURCE_FILES_PROPERTIES(${CFILES} PROPERTIES LANGUAGE CXX)
  set (CMAKE_CXX_STANDARD 20)
endif (CPP_COMPILE)

unset(CPP_COMPILE)

add_executable(echo_client client.c shared.c)
add_executable(echo_server server.c shared.c)

add_compile_options(-Wall -Wextra -Wpedantic)

if (ENABLE_TLS)
    message("Compile with TLS enabled")
    target_compile_definitions(echo_server PUBLIC NBN_TLS)
    target_compile_definitions(echo_client PUBLIC NBN_TLS)
endif (ENABLE_TLS)

unset(ENABLE_TLS)

if (WEBRTC_NATIVE_SERVER)
    if (EMSCRIPTEN)
        message(FATAL_ERROR "Cannot compile native webrtc driver with emscripten")
    endif (EMSCRIPTEN)

    message("Compile server with webrtc native driver")

    target_compile_definitions(echo_server PUBLIC NBN_WEBRTC_NATIVE)
    target_link_libraries(echo_server ${LIBDATACHANNEL_LIBRARY_PATH} m)
    target_include_directories(echo_server PUBLIC "${LIBDATACHANNEL_INCLUDE_PATH}")
endif (WEBRTC_NATIVE_SERVER)

unset(WEBRTC_NATIVE_SERVER)

if (WEBRTC_NATIVE_CLIENT)
    if (EMSCRIPTEN)
        message(FATAL_ERROR "Cannot compile native webrtc driver with emscripten")
    endif (EMSCRIPTEN)

    message("Compile client with webrtc native driver")

    target_compile_definitions(echo_client PUBLIC NBN_WEBRTC_NATIVE)
    target_link_libraries(echo_client ${LIBDATACHANNEL_LIBRARY_PATH} m)
    target_include_directories(echo_client PUBLIC "${LIBDATACHANNEL_INCLUDE_PATH}")
endif (WEBRTC_NATIVE_CLIENT)

unset(WEBRTC_NATIVE_CLIENT)

target_compile_definitions(echo_client PUBLIC NBN_DEBUG)
target_compile_definitions(echo_server PUBLIC NBN_DEBUG)

if(WIN32)
  target_link_libraries(echo_client ws2_32)
  target_link_libraries(echo_server ws2_32)
else()
  # link with pthread when we are not on windows
  target_link_libraries(echo_client pthread)
  target_link_libraries(echo_server pthread)
endif(WIN32)

if (UNIX)
  # link with libm on unix
  target_link_libraries(echo_client m)
  target_link_libraries(echo_server m)
endif (UNIX)

if (EMSCRIPTEN)
    set(ASYNCIFY_IMPORTS "[\"__js_game_server_start\", \"__js_game_client_start\", \"__js_game_client_close\"]")

    set_target_properties(echo_server PROPERTIES LINK_FLAGS "--js-library ${CMAKE_CURRENT_SOURCE_DIR}/../../net_drivers/webrtc/js/api.js \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s TOTAL_MEMORY=30MB \
    -s EXIT_RUNTIME=1 \
    -s ASSERTIONS=1 \
    -s ASYNCIFY \
    -s ASYNCIFY_IMPORTS=\"${ASYNCIFY_IMPORTS}\"")

    set_target_properties(echo_client PROPERTIES LINK_FLAGS "--js-library ${CMAKE_CURRENT_SOURCE_DIR}/../../net_drivers/webrtc/js/api.js \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s TOTAL_MEMORY=30MB \
    -s EXIT_RUNTIME=1 \
    -s ASSERTIONS=1 \
    -s ASYNCIFY \
    -s ASYNCIFY_IMPORTS=\"${ASYNCIFY_IMPORTS}\"")
endif()
