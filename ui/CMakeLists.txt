# ui：wxWidgets 表现层，唯一依赖 wx 的可执行目标。
include(${wxWidgets_USE_FILE})

add_executable(CircuitEditor
    src/main.cpp
)

target_link_libraries(CircuitEditor PRIVATE ${wxWidgets_LIBRARIES})