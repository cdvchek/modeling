cmake_minimum_required(VERSION 3.23)

project(modeling)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)

set(CORE_SRC
    src/core/font/bitmap_font.cpp
    src/core/font/embedded_fonts.cpp
    src/core/font/font_library.cpp
    src/core/time/frame_timer.cpp
    src/core/input/input_state.cpp
    src/core/input/action_map.cpp
    src/core/input/context_manager.cpp
    src/core/input/keybinds.cpp
    src/core/console/command_system.cpp
    src/core/console/console.cpp
    src/core/io/crc32.cpp
    src/ui/ui_draw_list.cpp
    src/ui/ui_context.cpp
    src/ui/ui_widgets.cpp
    src/ui/radial_layout.cpp
    src/core/math/vec2.cpp
    src/core/math/vec3.cpp
    src/core/math/vec4.cpp
    src/core/math/mat4.cpp
    src/scene/mesh/presets/from_polygons.cpp
    src/scene/mesh/presets/cube.cpp
    src/scene/mesh/presets/plane.cpp
    src/scene/mesh/presets/grid.cpp
    src/scene/mesh/presets/circle.cpp
    src/scene/mesh/presets/cylinder.cpp
    src/scene/mesh/presets/cone.cpp
    src/scene/mesh/presets/uv_sphere.cpp
    src/scene/mesh/presets/ico_sphere.cpp
    src/scene/mesh/presets/torus.cpp
    src/scene/mesh/mesh_data_access.cpp
    src/scene/mesh/mesh_data_queries.cpp
    src/scene/mesh/mesh_data_geometry.cpp
    src/scene/mesh/mesh_data_primitives.cpp
    src/scene/mesh/ops/mesh_data_ops.cpp
    src/scene/mesh/ops/mesh_data_merge.cpp
    src/scene/mesh/ops/mesh_data_dissolve.cpp
    src/scene/mesh/ops/mesh_data_bevel.cpp
    src/scene/mesh/ops/mesh_data_region.cpp
    src/scene/mesh/mesh_data_validate.cpp
    src/scene/mesh/mesh_data_gpu.cpp
    src/scene/mesh/mesh_data_shading.cpp
    src/scene/mesh/uv_unwrap.cpp
    src/scene/mesh/mesh_data_serialize.cpp
    src/scene/lights/light_collection.cpp
    src/scene/references/reference_collection.cpp
    src/scene/materials/material_collection.cpp
    src/scene/textures/texture_collection.cpp
    src/scene/textures/paint_targets.cpp
    src/scene/textures/layers.cpp
    src/scene/textures/brush.cpp
    src/scene/textures/paint_mask.cpp
    src/scene/objects/object_collection.cpp
    src/scene/objects/origin.cpp
    src/scene/selection/selection.cpp
    src/scene/selection/mesh_selection.cpp
    src/scene/picking/ray.cpp
    src/scene/picking/scene_queries.cpp
    src/scene/transform.cpp
    src/scene/camera.cpp
    src/scene/history.cpp
    src/project/project_file.cpp
    src/asset/asset_file.cpp
    src/asset/export_plan.cpp
)

set(APP_SRC
    src/main.cpp
    src/application/actions/checks/check_actions.cpp
    src/application/actions/checks/check_bevel.cpp
    src/application/actions/checks/check_console.cpp
    src/application/actions/checks/check_grab.cpp
    src/application/actions/checks/check_inset.cpp
    src/application/actions/checks/check_rotate.cpp
    src/application/actions/checks/check_scale.cpp
    src/application/actions/checks/check_selection.cpp
    src/application/application_actions.cpp
    src/application/actions/editing_actions.cpp
    src/application/ui/radial_menu.cpp
    src/application/viewport/reference_images.cpp
    src/application/viewport/pictures.cpp
    src/application/commands/reference_commands.cpp
    src/application/viewport/material_view.cpp
    src/application/commands/material_commands.cpp
    src/application/commands/texture_commands.cpp
    src/application/commands/layer_commands.cpp
    src/application/commands/shading_commands.cpp
    src/application/ui/stats_overlay.cpp
    src/application/ui/console_view.cpp
    src/application/tools/tool_guides.cpp
    src/application/application_commands.cpp
    src/application/commands/light_commands.cpp
    src/application/commands/object_commands.cpp
    src/application/actions/project_actions.cpp
    src/application/ui/modal_windows.cpp
    src/application/actions/asset_actions.cpp
    src/application/ui/status_bar.cpp
    src/application/viewport/light_markers.cpp
    src/application/viewport/origin_markers.cpp
    src/application/actions/origin_actions.cpp
    src/application/ui/main_panel.cpp
    src/application/viewport/object_meshes.cpp
    src/application/application_events.cpp
    src/application/application_render.cpp
    src/application/application_scene.cpp
    src/application/application.cpp
    src/application/workspace.cpp
    src/application/uv/uv_editor.cpp
    src/application/uv/uv_tools.cpp
    src/application/uv/uv_operations.cpp
    src/application/paint/paint_workspace.cpp
    src/application/ui/top_bar.cpp
    src/platform/keys/keys_win32.cpp
    src/platform/renderer/opengl_renderer_win32.cpp
    src/platform/window/window_event_callback_win32.cpp
    src/platform/window/window_win32.cpp
    src/platform/platform_win32.cpp
    src/renderer/opengl/opengl_font.cpp
    src/renderer/opengl/opengl_mesh.cpp
    src/renderer/opengl/opengl_renderer_common.cpp
    src/renderer/opengl/opengl_material_preview.cpp
    src/renderer/opengl/opengl_ui_renderer.cpp
    src/renderer/opengl/shaders/opengl_shader.cpp
    src/renderer/opengl/shaders/opengl_shader_library.cpp
    src/renderer/debug_renderer.cpp
    src/renderer/renderer.cpp
)

set(C_SRC
    ${CMAKE_SOURCE_DIR}/lib/glad/glad.c
)

# The .vlmobj asset format, shared with the Aevora engine; depends on nothing in this app
add_library(vlmobj STATIC shared/vlmobj/vlmobj.cpp)
target_include_directories(vlmobj PUBLIC ${CMAKE_SOURCE_DIR}/shared)

# Image files (PNG), shared the same way
add_library(image STATIC shared/image/png.cpp shared/image/png_write.cpp shared/image/inflate.cpp shared/image/deflate.cpp)
target_include_directories(image PUBLIC ${CMAKE_SOURCE_DIR}/shared)

add_library(modeling_core STATIC ${CORE_SRC})
target_link_libraries(modeling_core PUBLIC vlmobj image)

target_include_directories(modeling_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Lets #embed <...> find files under assets/ (see embedded_fonts.cpp).
target_compile_options(modeling_core PRIVATE --embed-dir=${CMAKE_SOURCE_DIR}/assets)

add_executable(modeling ${APP_SRC} ${C_SRC} app.rc app.ico)

target_link_libraries(modeling PRIVATE
    modeling_core
    opengl32
    dwmapi
    comdlg32
)

set_target_properties(modeling PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

enable_testing()

file(GLOB TEST_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/*.cpp ${CMAKE_SOURCE_DIR}/shared/*/tests/*.cpp)

add_executable(tests ${TEST_SRC})
target_link_libraries(tests PRIVATE modeling_core)
target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
# Lets tests find checked-in reference files
target_compile_definitions(tests PRIVATE SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_test(NAME tests COMMAND tests)
