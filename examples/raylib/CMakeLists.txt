cmake_minimum_required(VERSION 3.0)

project(raylib_example C)

set(raylib_DIR cmake)

set(CLIENT_SOURCES client.c shared.c)
set(SERVER_SOURCES server.c shared.c)

add_compile_options(-Wall -Wextra -Wpedantic -Wno-unknown-pragmas -Wno-type-limits -std=c99)

add_executable(client ${CLIENT_SOURCES})
add_executable(server ${SERVER_SOURCES})

# set RAYLIB_LIBRARY_PATH from command line

target_link_libraries(client ${RAYLIB_LIBRARY_PATH} pthread m)
target_link_libraries(server ${RAYLIB_LIBRARY_PATH} pthread m)

# set RAYLIB_INCLUDE_PATH from command line

target_include_directories(client PUBLIC "${RAYLIB_INCLUDE_PATH}")
target_include_directories(server PUBLIC "${RAYLIB_INCLUDE_PATH}")

if(WIN32)
    target_link_libraries(client wsock32 ws2_32 opengl32 gdi32 winmm)
    target_link_libraries(server wsock32 ws2_32)
endif()

target_compile_definitions(client PUBLIC NBN_DEBUG)
target_compile_definitions(server PUBLIC NBN_DEBUG)
target_compile_definitions(server PUBLIC NBN_RAYLIB_SERVER)

if (ENCRYPTION_ENABLED)
  message("Encryption enabled")

  target_compile_definitions(client PUBLIC EXAMPLE_ENCRYPTION)
  target_compile_definitions(server PUBLIC EXAMPLE_ENCRYPTION)
endif(ENCRYPTION_ENABLED)

unset(ENCRYPTION_ENABLED)

# compile with C WebRTC driver
if (WEBRTC_C_DRIVER)
  # can't compile WebRTC native driver with emscripten
  if (EMSCRIPTEN)
    message(SEND_ERROR "Can't compile WebRTC native driver with emscripten")
  endif (EMSCRIPTEN)

  message("Compiling with C WebRTC driver")

  target_compile_definitions(server PUBLIC SOAK_WEBRTC_C_DRIVER)

  target_link_libraries(server ${LIBFACILIO_LIBRARY_PATH})
  target_link_libraries(server ${LIBCRYPTO_LIBRARY_PATH})
  target_link_libraries(server ${LIBSSL_LIBRARY_PATH})
  target_link_libraries(server ${LIBDATACHANNEL_LIBRARY_PATH})

  target_include_directories(server PUBLIC "${LIBFACILIO_INCLUDE_PATH}")
  target_include_directories(server PUBLIC "${OPENSSL_INCLUDE_PATH}")
  target_include_directories(server PUBLIC "${LIBDATACHANNEL_INCLUDE_PATH}")

  if (USE_HTTPS)
    target_compile_definitions(server PUBLIC NBN_HTTPS_SERVER_NAME="localhost")
    target_compile_definitions(server PUBLIC NBN_HTTPS_KEY_PEM="localhost.key")
    target_compile_definitions(server PUBLIC NBN_HTTPS_CERT_PEM="localhost.crt")
  endif (USE_HTTPS)
endif (WEBRTC_C_DRIVER)

unset(WEBRTC_C_DRIVER)

# Use HTTPS (for WebRTC drivers)
if (USE_HTTPS)
  message("Compiling with HTTPS enabled")

  target_compile_definitions(client PUBLIC NBN_USE_HTTPS)
  target_compile_definitions(server PUBLIC NBN_USE_HTTPS)
endif (USE_HTTPS)

unset(USE_HTTPS)

if (EMSCRIPTEN)
    set(ASYNCIFY_IMPORTS "[\"__js_game_server_start\", \"__js_game_client_start\", \"__js_game_client_close\"]")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s ALLOW_MEMORY_GROWTH=1 -s TOTAL_MEMORY=30MB -s EXIT_RUNTIME=1 -s ASSERTIONS=1 -s ASYNCIFY -s ASYNCIFY_IMPORTS=\"${ASYNCIFY_IMPORTS}\"")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --js-library ${CMAKE_CURRENT_SOURCE_DIR}/../../net_drivers/webrtc/js/api.js")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_GLFW=3 --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/shell.html")
    set_target_properties(client PROPERTIES SUFFIX ".html")

    add_custom_command(
        TARGET client
        POST_BUILD
        COMMAND browserify ${CMAKE_CURRENT_SOURCE_DIR}/../../net_drivers/webrtc/js/nbnet.js -o nbnet_bundle.js)
endif()

if (APPLE)
  target_link_libraries(client "-framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo")
endif (APPLE)
