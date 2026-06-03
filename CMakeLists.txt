
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
    src/kameorepowered_hooks_combat.cpp
    src/kameorepowered_hooks_audio.cpp
    src/kameorepowered_hooks_dlc.cpp
    ${GENERATED_SOURCES}
)

if(WIN32)
    add_executable(kameorepowered WIN32 ${KAMEOREPOWERED_SOURCES} icon/app.rc)
else()
    add_executable(kameorepowered ${KAMEOREPOWERED_SOURCES})
    set_target_properties(kameorepowered PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()

rexglue_setup_target(kameorepowered)
