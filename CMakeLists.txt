
# kameo - ReXGlue Recompiled Project
#
# This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(kameo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Embed app icon
set(ICON_SRC "${CMAKE_CURRENT_SOURCE_DIR}/icon/app.ico")
set(ICON_HDR "${CMAKE_CURRENT_BINARY_DIR}/icon_data.h")
add_custom_command(
    OUTPUT "${ICON_HDR}"
    COMMAND xxd -i -n kameo_icon_data "${ICON_SRC}" > "${ICON_HDR}"
    DEPENDS "${ICON_SRC}"
    COMMENT "Embedding app.ico"
)
add_custom_target(kameo_icon DEPENDS "${ICON_HDR}")

# Sources
set(KAMEO_SOURCES
    src/main.cpp
    src/kameorepowered_hooks.cpp
    ${GENERATED_SOURCES}
)

if(WIN32)
    add_executable(kameo WIN32 ${KAMEO_SOURCES})
else()
    add_executable(kameo ${KAMEO_SOURCES})
endif()

rexglue_setup_target(kameo)

add_dependencies(kameo kameo_icon)
target_include_directories(kameo PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_options(kameo PRIVATE
    -include "${CMAKE_CURRENT_SOURCE_DIR}/src/kameorepowered_hooks.h"
)
