cmake_minimum_required(VERSION 3.23)

project(modeling)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)

set(CORE_SRC
    src/core/font/bitmap_font.cpp
    src/core/font/embedded_fonts.cpp
    src/core/math/vec2.cpp
    src/core/math/vec3.cpp
    src/core/math/vec4.cpp
    src/core/math/mat4.cpp
    src/scene/mesh/presets/cube.cpp
    src/scene/mesh/mesh_data_access.cpp
    src/scene/mesh/mesh_data_queries.cpp
    src/scene/mesh/mesh_data_geometry.cpp
    src/scene/mesh/mesh_data_primitives.cpp
    src/scene/mesh/mesh_data_ops.cpp
    src/scene/mesh/mesh_data_merge.cpp
    src/scene/mesh/mesh_data_dissolve.cpp
    src/scene/mesh/mesh_data_bevel.cpp
    src/scene/mesh/mesh_data_validate.cpp
    src/scene/mesh/mesh_data_gpu.cpp
)

set(APP_SRC
    src/main.cpp
    src/application/action_checks/check_actions.cpp
    src/application/action_checks/check_bevel.cpp
    src/application/action_checks/check_console.cpp
    src/application/action_checks/check_grab.cpp
    src/application/action_checks/check_rotate.cpp
    src/application/action_checks/check_scale.cpp
    src/application/action_checks/check_selection.cpp
    src/application/application_actions.cpp
    src/application/application_commands.cpp
    src/application/application_events.cpp
    src/application/application_render.cpp
    src/application/application_scene.cpp
    src/application/application.cpp
    src/core/console/command_system.cpp
    src/core/console/console.cpp
    src/core/input/input_state.cpp
    src/core/input/action_map.cpp
    src/core/input/context_manager.cpp
    src/platform/keys/keys_win32.cpp
    src/platform/renderer/opengl_renderer_win32.cpp
    src/platform/window/window_event_callback_win32.cpp
    src/platform/window/window_win32.cpp
    src/platform/platform_win32.cpp
    src/renderer/opengl/opengl_font.cpp
    src/renderer/opengl/opengl_mesh.cpp
    src/renderer/opengl/opengl_renderer_common.cpp
    src/renderer/opengl/opengl_shader.cpp
    src/renderer/debug_renderer.cpp
    src/renderer/renderer.cpp
    src/scene/selection/ray.cpp
    src/scene/selection/scene_queries.cpp
    src/scene/selection/selection.cpp
    src/scene/camera.cpp
    src/scene/object_collection.cpp
    src/scene/transform.cpp
)

set(C_SRC
    ${CMAKE_SOURCE_DIR}/lib/glad/glad.c
)

add_library(modeling_core STATIC ${CORE_SRC})

target_include_directories(modeling_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Lets #embed <...> find files under assets/ (see embedded_fonts.cpp).
target_compile_options(modeling_core PRIVATE --embed-dir=${CMAKE_SOURCE_DIR}/assets)

add_executable(modeling ${APP_SRC} ${C_SRC})

target_link_libraries(modeling PRIVATE
    modeling_core
    opengl32
    dwmapi
)

set_target_properties(modeling PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

enable_testing()

file(GLOB TEST_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/*.cpp)

add_executable(tests ${TEST_SRC})
target_link_libraries(tests PRIVATE modeling_core)

add_test(NAME tests COMMAND tests)
