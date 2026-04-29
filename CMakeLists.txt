
# kameorepowered - ReXGlue Recompiled Project
#
# This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(kameorepowered LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(KAMEOREPOWERED_SOURCES
    src/main.cpp
    src/kameorepowered_hooks.cpp
    ${GENERATED_SOURCES}
)

if(WIN32)
    add_executable(kameorepowered WIN32 ${KAMEOREPOWERED_SOURCES})
else()
    add_executable(kameorepowered ${KAMEOREPOWERED_SOURCES})
endif()

rexglue_setup_target(kameorepowered)

target_compile_options(kameorepowered PRIVATE
    -include "${CMAKE_CURRENT_SOURCE_DIR}/src/kameorepowered_hooks.h"
)
