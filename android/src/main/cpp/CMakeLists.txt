# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# libamberlith.so: Amberlith's engine and the cases every frontend runs (source/portable.txt), with
# Android's frontend and its own native cases: bionic's own, JNI and GLES. C only, so no C++
# runtime ships. build.gradle names the build (AMBERLITH_VERSION, AMBERLITH_BUILD, AMBERLITH_HASH),
# as the Makefile does for the NRO.
cmake_minimum_required(VERSION 3.22)
project(amberlith C)

foreach(name AMBERLITH_VERSION AMBERLITH_BUILD AMBERLITH_HASH)
    if(NOT DEFINED ${name})
        message(FATAL_ERROR "${name} is not set: android/build.gradle passes it, from VERSION and git")
    endif()
endforeach()

set(root "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
file(STRINGS "${root}/source/portable.txt" portable REGEX "^[^#]")
list(TRANSFORM portable PREPEND "${root}/")

add_library(amberlith SHARED ${portable} frontend.c platform_android.c probe.c bionic.c jni_cases.c
                             thread_end.c gles.c net.c)
target_include_directories(amberlith PRIVATE "${root}/source")
target_compile_definitions(amberlith PRIVATE AMBERLITH_VERSION="${AMBERLITH_VERSION}"
                           AMBERLITH_BUILD="${AMBERLITH_BUILD}" AMBERLITH_HASH="${AMBERLITH_HASH}")
# The engine's flags, as host.mk gives them: C11, every warning an error
target_compile_options(amberlith PRIVATE -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter)
# The fluid demo's reference rounds each multiply apart from its add, as the console's Makefile
# builds it (demo_fluidref.h)
set_source_files_properties("${root}/source/demo_fluidref.c" PROPERTIES COMPILE_OPTIONS
                            -ffp-contract=off)
target_link_libraries(amberlith PRIVATE EGL GLESv3 android log m dl)
