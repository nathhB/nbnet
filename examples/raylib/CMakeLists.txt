cmake_minimum_required(VERSION 3.0)

project(raylib_example C)

set(raylib_DIR cmake)

set(CLIENT_SOURCES client.c shared.c)
set(SERVER_SOURCES server.c shared.c)

add_compile_options(-Wall -Wextra -Wno-unknown-pragmas -Wno-type-limits)

add_executable(client ${CLIENT_SOURCES})
add_executable(server ${SERVER_SOURCES})

# --- RAYLIB ---

# set RAYLIB_LIBRARY_PATH from command line

find_library(RAYLIB_LIBRARY raylib HINTS ${RAYLIB_LIBRARY_PATH} REQUIRED)

message("Found raylib library: ${RAYLIB_LIBRARY}")

target_link_libraries(client ${RAYLIB_LIBRARY})
target_link_libraries(server ${RAYLIB_LIBRARY})

if(WIN32)
        target_link_libraries(client opengl32 gdi32 winmm)
        target_link_libraries(server opengl32 gdi32 winmm)
endif()

# set RAYLIB_INCLUDE_PATH from command line

target_include_directories(client PUBLIC "${RAYLIB_INCLUDE_PATH}")
target_include_directories(server PUBLIC "${RAYLIB_INCLUDE_PATH}")

# ---------

# --- NET DRIVERS ---

option(NBN_DRIVER_UDP "use udp driver")
option(NBN_DRIVER_UDP_SDL_NET "use SDL_net udp driver")

if (NBN_DRIVER_UDP_SDL_NET)
    message("Compile with SDL_net UDP network driver")

    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/sdl2)

    find_package(SDL2_net REQUIRED)

    target_compile_definitions(client PUBLIC NBN_DRIVER_UDP_SDL_NET)
    target_compile_definitions(server PUBLIC NBN_DRIVER_UDP_SDL_NET)
    
    target_link_libraries(client SDL2::Net)
    target_link_libraries(server SDL2::Net)
endif (NBN_DRIVER_UDP_SDL_NET)

if (NBN_DRIVER_UDP)
    message("Compile with UDP network driver")

    target_compile_definitions(client PUBLIC NBN_DRIVER_UDP)
    target_compile_definitions(server PUBLIC NBN_DRIVER_UDP)

    if(WIN32)
        target_link_libraries(client wsock32 ws2_32)
        target_link_libraries(server wsock32 ws2_32)
    endif()

endif (NBN_DRIVER_UDP)

# ---------

target_compile_definitions(client PUBLIC NBN_GAME_CLIENT NBN_DEBUG NBN_USE_PACKET_SIMULATOR)
target_compile_definitions(server PUBLIC NBN_GAME_SERVER NBN_DEBUG NBN_USE_PACKET_SIMULATOR)
