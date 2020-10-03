cmake_minimum_required(VERSION 3.0)

project(raylib_example C)

set(raylib_DIR cmake)

set(CLIENT_SOURCES client.c shared.c)
set(SERVER_SOURCES server.c shared.c)

add_compile_options(-Wall -Wextra -Wno-unknown-pragmas -Wno-type-limits)

add_executable(client ${CLIENT_SOURCES})
add_executable(server ${SERVER_SOURCES})

# set RAYLIB_LIBRARY_PATH from command line

find_library(RAYLIB_LIBRARY raylib HINTS ${RAYLIB_LIBRARY_PATH} REQUIRED)

message("Found raylib library: ${RAYLIB_LIBRARY}")

target_link_libraries(client ${RAYLIB_LIBRARY} pthread m)
target_link_libraries(server ${RAYLIB_LIBRARY} pthread m)

# set RAYLIB_INCLUDE_PATH from command line

target_include_directories(client PUBLIC "${RAYLIB_INCLUDE_PATH}")
target_include_directories(server PUBLIC "${RAYLIB_INCLUDE_PATH}")

if(WIN32)
    target_link_libraries(client wsock32 ws2_32 opengl32 gdi32 winmm)
    target_link_libraries(server wsock32 ws2_32)
endif()

target_compile_definitions(client PUBLIC NBN_GAME_CLIENT NBN_DEBUG NBN_USE_PACKET_SIMULATOR)
target_compile_definitions(server PUBLIC NBN_GAME_SERVER NBN_DEBUG NBN_USE_PACKET_SIMULATOR)

if (EMSCRIPTEN)
    set(ASYNCIFY_IMPORTS "[\"__js_game_server_start\", \"__js_game_client_start\", \"__js_game_client_close\"]")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_PTHREADS=1 -s ALLOW_MEMORY_GROWTH=1 -s EXIT_RUNTIME=1 -s ASSERTIONS=1 -s ASYNCIFY -s ASYNCIFY_IMPORTS=\"${ASYNCIFY_IMPORTS}\"")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --js-library ${CMAKE_CURRENT_SOURCE_DIR}/../../net_drivers/webrtc/js/api.js")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_GLFW=3 --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/shell.html")
    set_target_properties(client PROPERTIES SUFFIX ".html")

    add_custom_command(
        TARGET client
        POST_BUILD
        COMMAND browserify ${CMAKE_CURRENT_SOURCE_DIR}/../../net_drivers/webrtc/js/nbnet.js -o nbnet_bundle.js)
endif()
