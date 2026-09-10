# core：纯逻辑静态库，严格不链接/不包含任何 wxWidgets。
add_library(editor_core STATIC
    src/core.cpp
)

target_include_directories(editor_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)