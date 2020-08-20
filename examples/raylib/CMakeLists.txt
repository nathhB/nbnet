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

target_link_libraries(client ${RAYLIB_LIBRARY})
target_link_libraries(server ${RAYLIB_LIBRARY})

if(WIN32)
        target_link_libraries(client wsock32 ws2_32 opengl32 gdi32 winmm)
        target_link_libraries(server wsock32 ws2_32 opengl32 gdi32 winmm)
endif()

# set RAYLIB_INCLUDE_PATH from command line

target_include_directories(client PUBLIC "${RAYLIB_INCLUDE_PATH}")

target_compile_definitions(client PUBLIC NBN_GAME_CLIENT NBN_DEBUG)
target_compile_definitions(server PUBLIC NBN_GAME_SERVER NBN_DEBUG)