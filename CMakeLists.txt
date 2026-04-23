# rekameo - ReXGlue Recompiled Project (Kameo: Elements of Power)
#
# This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(rekameo LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(REKAMEO_SOURCES
    src/main.cpp
    src/rekameo_app.h
    src/kameo_engine/hooks.cpp
    src/kameo_engine/hooks.h
    src/kameo_engine/Log.h
    src/kameo_engine/rex_macros.h
    src/kameo_engine/Overlays/Fps.cpp
    src/kameo_engine/Overlays/Fps.h
)

if(WIN32)
    list(APPEND REKAMEO_SOURCES icon/app.rc)
    add_executable(rekameo WIN32 ${REKAMEO_SOURCES})
else()
    add_executable(rekameo ${REKAMEO_SOURCES})
endif()

rexglue_setup_target(rekameo)

target_include_directories(rekameo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(rekameo PRIVATE
    -Wno-missing-template-arg-list-after-template-kw
)
