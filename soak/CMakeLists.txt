cmake_minimum_required(VERSION 3.0)

project(soak C)

add_executable(client client.c soak.c)
add_executable(server server.c soak.c)

add_compile_options(-Wall -Wextra)

target_compile_definitions(client PUBLIC NBN_GAME_CLIENT NBN_DEBUG)
target_compile_definitions(server PUBLIC NBN_GAME_SERVER NBN_DEBUG)

if (NBN_DRIVER_UDP_SDL_NET)
    message("Compile with SDL_net network driver")

    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl2)

    find_package(SDL2_net REQUIRED)

    target_compile_definitions(client PUBLIC NBN_DRIVER_UDP_SDL_NET_IMPL)
    target_compile_definitions(server PUBLIC NBN_DRIVER_UDP_SDL_NET_IMPL)
    
    target_link_libraries(client SDL2::Net)
    target_link_libraries(server SDL2::Net)
endif(NBN_DRIVER_UDP_SDL_NET)
