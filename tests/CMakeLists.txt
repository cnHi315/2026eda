# tests：仅依赖 core，验证 core 层无 wx 依赖即可测试。
add_executable(editor_core_tests
    test_core.cpp
)

target_link_libraries(editor_core_tests PRIVATE
    editor_core
    GTest::gtest_main
)

add_test(NAME core_tests COMMAND editor_core_tests)