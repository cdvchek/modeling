# Aevora Engine: the engine library, the editor built on it, and their tests

# Everything the game and the editor share; no OpenGL or Win32, so it can be tested on its own
add_library(aevora_engine STATIC
    engine/jobs/job_system.cpp
    engine/project/project.cpp
)
target_link_libraries(aevora_engine PUBLIC core)
target_include_directories(aevora_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(aevora
    editor/main.cpp
    editor/editor.cpp
    editor/editor_actions.cpp
    editor/editor_ui.cpp
)

target_link_libraries(aevora PRIVATE
    aevora_engine
    ui
    platform
    gfx
)

set_target_properties(aevora PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

file(GLOB AEVORA_TEST_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)

add_executable(aevora_tests ${AEVORA_TEST_SRC})
target_link_libraries(aevora_tests PRIVATE test_runner aevora_engine)
set_target_properties(aevora_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_test(NAME aevora_tests COMMAND aevora_tests)
