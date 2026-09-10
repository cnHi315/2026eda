cmake_minimum_required(VERSION 3.16)
project(CircuitEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# wxWidgets
# ---------------------------------------------------------------------------
# core/base 供 MVT 使用，aui/propgrid/stc 已就绪，后续节点（属性表/树形库等）启用。
set(wxWidgets_USE_UNICODE ON)
find_package(wxWidgets REQUIRED COMPONENTS core base aui propgrid stc)

# ---------------------------------------------------------------------------
# 测试框架：GoogleTest（无 sudo 场景下通过 FetchContent 拉取）
# ---------------------------------------------------------------------------
include(FetchContent)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
FetchContent_MakeAvailable(googletest)

enable_testing()

# ---------------------------------------------------------------------------
# 共享契约头文件：core_interfaces.h 由 ui 与 core 共同引用。
# 声明为目录级 include 路径，故 core / ui / tests 均可直接 #include "core_interfaces.h"。
# ---------------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 依赖方向约束：ui -> core，core 不得依赖任何 wx。
add_subdirectory(core)
add_subdirectory(ui)
add_subdirectory(tests)