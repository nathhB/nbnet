cmake_minimum_required(VERSION 3.1)

project(rpc)

option(CPP_COMPILE OFF)

# allow to compile as cpp
if (CPP_COMPILE)
  file(GLOB_RECURSE CFILES "${CMAKE_SOURCE_DIR}/*.c")
  SET_SOURCE_FILES_PROPERTIES(${CFILES} PROPERTIES LANGUAGE CXX)
  set (CMAKE_CXX_STANDARD 20)
endif (CPP_COMPILE)

unset(CPP_COMPILE)

add_executable(rpc_client client.c shared.c)
add_executable(rpc_server server.c shared.c)

add_compile_options(-Wall -Wextra -Wpedantic)

target_compile_definitions(rpc_client PUBLIC NBN_DEBUG)
target_compile_definitions(rpc_server PUBLIC NBN_DEBUG)

option(ENCRYPTION_ENABLED OFF)
option(WEBRTC_DRIVER_C OFF)

if(WIN32)
  target_link_libraries(rpc_client wsock32 ws2_32)
  target_link_libraries(rpc_server wsock32 ws2_32)
else()
  # link with pthread when we are not on windows
  target_link_libraries(rpc_client pthread)
  target_link_libraries(rpc_server pthread)
endif(WIN32)

if (UNIX)
  # link with libm on unix
  target_link_libraries(rpc_client m)
  target_link_libraries(rpc_server m)
endif (UNIX)

if (EMSCRIPTEN)
    set(ASYNCIFY_IMPORTS "[\"__js_game_server_start\", \"__js_game_client_start\", \"__js_game_client_close\"]")

    set_target_properties(rpc_server PROPERTIES LINK_FLAGS "--js-library ${CMAKE_CURRENT_SOURCE_DIR}/../../net_drivers/webrtc/js/api.js \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s TOTAL_MEMORY=30MB \
    -s EXIT_RUNTIME=1 \
    -s ASSERTIONS=1 \
    -s ASYNCIFY \
    -s ASYNCIFY_IMPORTS=\"${ASYNCIFY_IMPORTS}\"")

    set_target_properties(rpc_client PROPERTIES LINK_FLAGS "--js-library ${CMAKE_CURRENT_SOURCE_DIR}/../../net_drivers/webrtc/js/api.js \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s TOTAL_MEMORY=30MB \
    -s EXIT_RUNTIME=1 \
    -s ASSERTIONS=1 \
    -s ASYNCIFY \
    -s ASYNCIFY_IMPORTS=\"${ASYNCIFY_IMPORTS}\"")
endif()
