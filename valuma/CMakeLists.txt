# Valuma Studio, the modeling program

set(VALUMA_CORE_SRC
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

set(VALUMA_APP_SRC
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

    src/renderer/opengl/opengl_mesh.cpp
    src/renderer/opengl/opengl_renderer_common.cpp

    src/renderer/opengl/opengl_material_preview.cpp

    src/renderer/opengl/shaders/opengl_shader_library.cpp
    src/renderer/debug_renderer.cpp
    src/renderer/renderer.cpp
)

# The scene, the project file format, and .vlmobj baking; no OpenGL or Win32, so it can be tested on its own
add_library(valuma_core STATIC ${VALUMA_CORE_SRC})
target_link_libraries(valuma_core PUBLIC core ui vlmobj image)
target_include_directories(valuma_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(valuma ${VALUMA_APP_SRC} app.rc app.ico)

target_link_libraries(valuma PRIVATE
    valuma_core
    platform
    gfx
    opengl32
    dwmapi
)

set_target_properties(valuma PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

file(GLOB VALUMA_TEST_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)

add_executable(valuma_tests ${VALUMA_TEST_SRC})
target_link_libraries(valuma_tests PRIVATE test_runner valuma_core)
target_include_directories(valuma_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
# Lets tests find checked-in reference files
target_compile_definitions(valuma_tests PRIVATE SOURCE_DIR="${CMAKE_SOURCE_DIR}")
set_target_properties(valuma_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_test(NAME valuma_tests COMMAND valuma_tests)
