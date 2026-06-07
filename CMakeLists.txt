cmake_minimum_required(VERSION 3.23)

project(modeling)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)

set(COMMON_SRC
    src/main.cpp
    src/application/application.cpp
    src/platform/window/window_win32.cpp
    src/platform/window/window_event_callback_win32.cpp
    src/platform/platform_win32.cpp
    src/platform/keys/keys_win32.cpp
    src/platform/renderer/opengl_renderer_win32.cpp
    src/renderer/opengl/opengl_renderer_common.cpp
    src/renderer/renderer.cpp
    src/renderer/opengl/opengl_shader.cpp
    src/renderer/opengl/opengl_mesh.cpp
    src/core/input/input_state.cpp
    src/core/input/action_map.cpp
    src/core/math/vec2.cpp
    src/core/math/vec3.cpp
    src/core/math/vec4.cpp
    src/core/math/mat4.cpp
)

set(C_SRC
    ${CMAKE_SOURCE_DIR}/lib/glad/glad.c
)

add_executable(modeling ${COMMON_SRC} ${C_SRC})

target_include_directories(modeling PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(modeling PRIVATE
    opengl32
    dwmapi
)

set_target_properties(modeling PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)