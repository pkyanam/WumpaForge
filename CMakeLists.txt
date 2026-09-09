cmake_minimum_required(VERSION 3.20)
project(wrath_native LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "Native Apple Silicon target")
endif()
add_subdirectory(third_party/xboxrecomp)
option(WRATH_COMPONENT_CHECKS_ONLY "Configure component checks without generated game code" OFF)
if(NOT WRATH_COMPONENT_CHECKS_ONLY)
    file(GLOB GENERATED CONFIGURE_DEPENDS "local/generated/*.c")
    if(NOT GENERATED)
        message(FATAL_ERROR "Generate the game C sources first with tools/pipeline.py")
    endif()
    add_executable(wrath_native src/main.c src/overrides.c src/graphics.c src/input_bridge.c src/audio_bridge.c src/crt_bridge.c src/timing_bridge.c src/nv2a_vertex.c src/nv2a_vertex_input.c src/nv2a_pixel.c ${GENERATED})
    target_include_directories(wrath_native PRIVATE local/generated)
    target_link_libraries(wrath_native PRIVATE xboxrecomp)
    target_link_directories(wrath_native PRIVATE /opt/homebrew/lib)
    target_include_directories(wrath_native PRIVATE /opt/homebrew/include)
    target_link_libraries(wrath_native PRIVATE epoxy)
    # Machine-lifted code intentionally aliases memory through different types.
    target_compile_options(wrath_native PRIVATE -fno-strict-aliasing -fwrapv
        -Wno-unused-label -Wno-unused-variable)
    set_source_files_properties(${GENERATED} PROPERTIES COMPILE_OPTIONS "-O1;-gline-tables-only")

endif()

# Explicit component targets never add work to the normal game build.
# Keep assertions enabled even in release configurations: they are the oracles.
if(APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_executable(wrath_graphics_check EXCLUDE_FROM_ALL
        src/graphics.c src/nv2a_vertex.c src/nv2a_vertex_input.c src/nv2a_pixel.c)
    target_compile_definitions(wrath_graphics_check PRIVATE WRATH_GRAPHICS_SMOKE_TEST)
    target_compile_options(wrath_graphics_check PRIVATE -UNDEBUG -O1)
    target_include_directories(wrath_graphics_check PRIVATE /opt/homebrew/include)
    target_link_directories(wrath_graphics_check PRIVATE /opt/homebrew/lib)
    target_link_libraries(wrath_graphics_check PRIVATE xbox_d3d8 epoxy)

    add_executable(wrath_filter_check EXCLUDE_FROM_ALL tools/test_presentation_filter.c)
    target_compile_options(wrath_filter_check PRIVATE -UNDEBUG -O2)
    target_link_libraries(wrath_filter_check PRIVATE "-framework OpenGL")
endif()
