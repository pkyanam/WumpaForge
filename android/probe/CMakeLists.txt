cmake_minimum_required(VERSION 3.20)
project(wumpaforge_android_memory_probe LANGUAGES C)
include(${CMAKE_CURRENT_LIST_DIR}/target_contract.cmake)

# This executable is a diagnostic, not the game, an app library or an APK.
add_executable(wumpaforge_memory_probe memory_probe.c android_backing.c)
target_compile_features(wumpaforge_memory_probe PRIVATE c_std_11)
target_compile_definitions(wumpaforge_memory_probe PRIVATE _GNU_SOURCE)
target_compile_options(wumpaforge_memory_probe PRIVATE -Wall -Wextra -Werror)
target_link_options(wumpaforge_memory_probe PRIVATE
    "-Wl,-z,max-page-size=16384" "-Wl,-z,common-page-size=16384")
