cmake_minimum_required(VERSION 3.16)
project(CircuitEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC: 源文件含中文(UTF-8),用 /utf-8 让编译器按 UTF-8 解析源码与生成字符串字面量,
# 避免运行时终端中文乱码与 C4819 警告。
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ---------------------------------------------------------------------------
# wxWidgets(仅程序界面需要)
# 顺序不能改:propgrid 必须排在 core 前面。
# wxPropertyGrid 引用了 core 里的 wxOwnerDrawnComboBox,静态库按从左到右解析,
# 排反了就 undefined reference —— 只在 MinGW/Windows 暴露,Linux 走 wx-config 不受影响。
# ---------------------------------------------------------------------------
set(wxWidgets_USE_UNICODE ON)
find_package(wxWidgets REQUIRED COMPONENTS propgrid core base)
include(${wxWidgets_USE_FILE})

# ---------------------------------------------------------------------------
# 契约头文件:src/ 对整个项目可见,各模块用 #include "contract/data_model.h" 引用
# ---------------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# ---------------------------------------------------------------------------
# 源码自动收集:新加 .cpp 不需要改本文件,重新 cmake 即可
# ---------------------------------------------------------------------------
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    src/main.cpp
    src/model/*.cpp
    src/ui/*.cpp
    src/components/*.cpp
    src/io/*.cpp
    src/simulation/*.cpp
)

add_executable(CircuitEditor WIN32 ${APP_SOURCES})
set_target_properties(CircuitEditor PROPERTIES WIN32_EXECUTABLE TRUE)
target_link_libraries(CircuitEditor ${wxWidgets_LIBRARIES})

# ---------------------------------------------------------------------------
# 控制台测试目标:运行仿真器真值表自测
#   cmake --build build
#   .\build\Debug\sim_test.exe
# ---------------------------------------------------------------------------
add_executable(sim_test tests/main_test.cpp src/simulation/simulator.cpp)
