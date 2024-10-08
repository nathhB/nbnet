cmake_minimum_required(VERSION 3.1)

project(echo_bytes)

option(CPP_COMPILE OFF)

# allow to compile as cpp
if (CPP_COMPILE)
  file(GLOB_RECURSE CFILES "${CMAKE_SOURCE_DIR}/*.c")
  SET_SOURCE_FILES_PROPERTIES(${CFILES} PROPERTIES LANGUAGE CXX)
  set (CMAKE_CXX_STANDARD 20)
endif (CPP_COMPILE)

unset(CPP_COMPILE CACHE)

add_compile_options(-Wall)

if(CMAKE_COMPILER_IS_GNUCXX)
    add_compile_options(-Wextra -Wpedantic)
endif (CMAKE_COMPILER_IS_GNUCXX)

add_executable(echo_bytes_client client.c shared.c)
add_executable(echo_bytes_server server.c shared.c)

target_compile_definitions(echo_bytes_client PUBLIC NBN_DEBUG)
target_compile_definitions(echo_bytes_server PUBLIC NBN_DEBUG)

if(WIN32)
  target_link_libraries(echo_bytes_client wsock32 ws2_32)
  target_link_libraries(echo_bytes_server wsock32 ws2_32)
else()
  # link with pthread when we are not on windows
  target_link_libraries(echo_bytes_client pthread)
  target_link_libraries(echo_bytes_server pthread)
endif(WIN32)

if (UNIX)
  # link with libm on unix
  target_link_libraries(echo_bytes_client m)
  target_link_libraries(echo_bytes_server m)
endif (UNIX)

if (EMSCRIPTEN)
    set(ASYNCIFY_IMPORTS "[\"__js_game_server_start\", \"__js_game_client_start\", \"__js_game_client_close\"]")

    set_target_properties(echo_bytes_server PROPERTIES LINK_FLAGS "--js-library ${CMAKE_CURRENT_SOURCE_DIR}/../../net_drivers/webrtc/js/api.js \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s TOTAL_MEMORY=30MB \
    -s EXIT_RUNTIME=1 \
    -s ASSERTIONS=1 \
    -s ASYNCIFY \
    -s ASYNCIFY_IMPORTS=\"${ASYNCIFY_IMPORTS}\"")

    set_target_properties(echo_bytes_client PROPERTIES LINK_FLAGS "--js-library ${CMAKE_CURRENT_SOURCE_DIR}/../../net_drivers/webrtc/js/api.js \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s TOTAL_MEMORY=30MB \
    -s EXIT_RUNTIME=1 \
    -s ASSERTIONS=1 \
    -s ASYNCIFY \
    -s ASYNCIFY_IMPORTS=\"${ASYNCIFY_IMPORTS}\"")
endif()
