cmake_minimum_required(VERSION 3.5)

project(soak)

option(CPP_COMPILE OFF)

# allow to compile as cpp
if (CPP_COMPILE)
  file(GLOB_RECURSE CFILES "${CMAKE_SOURCE_DIR}/*.c")
  SET_SOURCE_FILES_PROPERTIES(${CFILES} PROPERTIES LANGUAGE CXX)
endif (CPP_COMPILE)

unset(CPP_COMPILE)

add_executable(client client.c soak.c log.c cargs.c ../nbnet.c)
add_executable(server server.c soak.c log.c cargs.c ../nbnet.c)

add_compile_options(-Wall -Wextra -Wpedantic -std=c99)

if (CMAKE_BUILD_TYPE MATCHES "Debug")
    message("Compiling in Debug mode with packet simulator enabled")

    target_compile_definitions(client PUBLIC NBN_DEBUG NBN_USE_PACKET_SIMULATOR)
    target_compile_definitions(server PUBLIC NBN_DEBUG NBN_USE_PACKET_SIMULATOR)
endif()

target_compile_definitions(client PUBLIC SOAK_CLIENT)
target_compile_definitions(server PUBLIC SOAK_SERVER)
target_compile_definitions(client PUBLIC NBN_CHANNEL_COUNT=4)
target_compile_definitions(server PUBLIC NBN_CHANNEL_COUNT=4)

target_include_directories(client PUBLIC "../")
target_include_directories(server PUBLIC "../")

option(WEBRTC_NATIVE OFF)
option(UDP OFF)

# compile with UDP driver
if (UDP)
  # can't compile UDP driver with emscripten
  if (EMSCRIPTEN)
    message(SEND_ERROR "Can't compile UDP driver with emscripten")
  endif (EMSCRIPTEN)

  message("Compiling with UDP driver")

  target_compile_definitions(server PUBLIC NBN_UDP)
  target_compile_definitions(client PUBLIC NBN_UDP)
endif (UDP)

# compile with C WebRTC driver
if (WEBRTC_NATIVE)
  # can't compile WebRTC native driver with emscripten
  if (EMSCRIPTEN)
    message(SEND_ERROR "Can't compile WebRTC native driver with emscripten")
  endif (EMSCRIPTEN)

  message("Compiling server with WebRTC native driver")

  target_compile_definitions(server PUBLIC NBN_WEBRTC_NATIVE)

  target_link_libraries(server ${LIBDATACHANNEL_LIBRARY_PATH} m)

  target_include_directories(server PUBLIC "${LIBDATACHANNEL_INCLUDE_PATH}")

endif (WEBRTC_NATIVE)

unset(WEBRTC_NATIVE)

if(WIN32)
  target_link_libraries(client wsock32 ws2_32)
  target_link_libraries(server wsock32 ws2_32)
else()
  # link with pthread when we are not on windows
  target_link_libraries(client pthread)
  target_link_libraries(server pthread)
endif(WIN32)

if (UNIX)
  # link with libm on unix
  target_link_libraries(client m)
  target_link_libraries(server m)
endif (UNIX)

if (EMSCRIPTEN)
    set(ASYNCIFY_IMPORTS "[\"__js_game_server_start\", \"__js_game_client_start\", \"__js_game_client_close\"]")

    set_target_properties(server PROPERTIES LINK_FLAGS "--js-library ${CMAKE_CURRENT_SOURCE_DIR}/../webrtc/js/api.js \
    -s EXPORTED_RUNTIME_METHODS=[HEAPU8] \
    -s TOTAL_MEMORY=30MB \
    -s USE_PTHREADS=1 \
    -s PTHREAD_POOL_SIZE=4 \
    -s EXIT_RUNTIME=1 \
    -s RUNTIME_DEBUG=1 \
    -s ASSERTIONS=1 \
    -s ASYNCIFY \
    -s ASYNCIFY_IMPORTS=\"${ASYNCIFY_IMPORTS}\"")

    set_target_properties(client PROPERTIES LINK_FLAGS "--js-library ${CMAKE_CURRENT_SOURCE_DIR}/../webrtc/js/api.js \
    -s TOTAL_MEMORY=30MB \
    -s USE_PTHREADS=1 \
    -s PTHREAD_POOL_SIZE=4 \
    -s EXIT_RUNTIME=1 \
    -s ASSERTIONS=1 \
    -s RUNTIME_DEBUG=1 \
    -s ASYNCIFY \
    -s ASYNCIFY_IMPORTS=\"${ASYNCIFY_IMPORTS}\"")
endif()
