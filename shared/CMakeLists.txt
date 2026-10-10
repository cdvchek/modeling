# Code for every Aevora Works program; nothing here depends on any one of them

set(SHARED_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# The .vlmobj asset format; depends on nothing else
add_library(vlmobj STATIC vlmobj/vlmobj.cpp)
target_include_directories(vlmobj PUBLIC ${SHARED_DIR})

# Image files (PNG); depends on nothing else
add_library(image STATIC image/png.cpp image/png_write.cpp image/inflate.cpp image/deflate.cpp)
target_include_directories(image PUBLIC ${SHARED_DIR})

# Math, fonts, input, the console, and other building blocks; no OpenGL or Win32
add_library(core STATIC
    core/font/bitmap_font.cpp
    core/font/embedded_fonts.cpp
    core/font/font_library.cpp
    core/time/frame_timer.cpp
    core/input/input_state.cpp
    core/input/action_map.cpp
    core/input/context_manager.cpp
    core/input/keybinds.cpp
    core/console/command_system.cpp
    core/console/console.cpp
    core/io/crc32.cpp
    core/math/vec2.cpp
    core/math/vec3.cpp
    core/math/vec4.cpp
    core/math/mat4.cpp
)
target_include_directories(core PUBLIC ${SHARED_DIR} ${SHARED_DIR}/core/include)

# Lets #embed <...> find files under shared/assets/ (see embedded_fonts.cpp).
target_compile_options(core PRIVATE --embed-dir=${SHARED_DIR}/assets)

# The 2D draw list and widgets; no OpenGL or Win32
add_library(ui STATIC
    ui/ui_draw_list.cpp
    ui/ui_context.cpp
    ui/ui_widgets.cpp
    ui/radial_layout.cpp
)
target_link_libraries(ui PUBLIC core)

# The Win32 window, message pump, clipboard, and file dialogs
add_library(platform STATIC
    platform/keys/keys_win32.cpp
    platform/window/window_event_callback_win32.cpp
    platform/window/window_win32.cpp
    platform/platform_win32.cpp
)
target_link_libraries(platform PUBLIC core opengl32 comdlg32)

# OpenGL building blocks: the GLAD loader, the context, shaders, textures, fonts, and drawing the UI
add_library(gfx STATIC
    gfx/glad/glad.c
    gfx/opengl/opengl_context_win32.cpp
    gfx/opengl/opengl_shader.cpp
    gfx/opengl/opengl_texture.cpp
    gfx/opengl/opengl_font.cpp
    gfx/opengl/opengl_ui_renderer.cpp
)
target_include_directories(gfx PUBLIC ${SHARED_DIR}/gfx)
target_link_libraries(gfx PUBLIC core ui opengl32)

# The test runner (test.hpp and main); each tests executable links it
add_library(test_runner STATIC test/test_main.cpp)
target_include_directories(test_runner PUBLIC ${SHARED_DIR}/test)

file(GLOB SHARED_TEST_SRC CONFIGURE_DEPENDS ${SHARED_DIR}/*/tests/*.cpp)

add_executable(shared_tests ${SHARED_TEST_SRC})
target_link_libraries(shared_tests PRIVATE test_runner core ui vlmobj image)
# Lets tests find checked-in reference files
target_compile_definitions(shared_tests PRIVATE SOURCE_DIR="${CMAKE_SOURCE_DIR}")
set_target_properties(shared_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_test(NAME shared_tests COMMAND shared_tests)
