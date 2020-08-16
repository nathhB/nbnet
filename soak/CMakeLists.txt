cmake_minimum_required(VERSION 3.0)

project(soak C)

add_executable(client client.c soak.c logging.c)
add_executable(server server.c soak.c logging.c)

add_compile_options(-Wall -Wextra)

target_compile_definitions(client PUBLIC NBN_GAME_CLIENT NBN_DEBUG NBN_USE_PACKET_SIMULATOR)
target_compile_definitions(server PUBLIC NBN_GAME_SERVER NBN_DEBUG NBN_USE_PACKET_SIMULATOR)

option(NBN_DRIVER_UDP "use udp driver")
option(NBN_DRIVER_UDP_SDL_NET "use SDL_net udp driver")

if (NBN_DRIVER_UDP_SDL_NET)
    message("Compile with SDL_net network driver")

    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl2)

    find_package(SDL2_net REQUIRED)

    target_compile_definitions(client PUBLIC NBN_DRIVER_UDP_SDL_NET_IMPL)
    target_compile_definitions(server PUBLIC NBN_DRIVER_UDP_SDL_NET_IMPL)
    
    target_link_libraries(client SDL2::Net)
    target_link_libraries(server SDL2::Net)
endif (NBN_DRIVER_UDP_SDL_NET)

if (NBN_DRIVER_UDP)
    message("Compile with UDP network driver")

    target_compile_definitions(client PUBLIC NBN_DRIVER_UDP_IMPL)
    target_compile_definitions(server PUBLIC NBN_DRIVER_UDP_IMPL)

    if(WIN32)
        target_link_libraries(client wsock32 ws2_32)
        target_link_libraries(server wsock32 ws2_32)
    endif()

endif (NBN_DRIVER_UDP)
