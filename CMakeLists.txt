
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

# Title-update builds: the hand-written hooks call game functions by hardcoded
# vanilla addresses that the update relocates. Until those are re-derived for the
# patched image, -DKAMEO_TU=ON compiles the affected calls out (the custom hooks
# are also stripped from the TU codegen config). The regular build leaves it OFF
# and keeps all features. Set automatically by scripts/build.py --tu.
option(KAMEO_TU "Build for a baked-in title update (disables vanilla-address hooks)" OFF)
if(KAMEO_TU)
    target_compile_definitions(kameorepowered PRIVATE KAMEO_TU=1)
endif()
