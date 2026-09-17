cmake_minimum_required(VERSION 3.16)
project(CircuitEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# wxWidgets(仅程序界面需要;阶段一用到 PropertyGrid 时再加 propgrid 组件)
# ---------------------------------------------------------------------------
set(wxWidgets_USE_UNICODE ON)
find_package(wxWidgets REQUIRED COMPONENTS core base propgrid)
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

add_executable(CircuitEditor ${APP_SOURCES})
target_link_libraries(CircuitEditor ${wxWidgets_LIBRARIES})
