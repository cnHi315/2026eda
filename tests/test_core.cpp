#include <gtest/gtest.h>

#include "editor/core/core.h"

// MVT 测试链验收：证明 core 层可在无 GUI 环境独立运行测试。

TEST(CoreSanity, AddWorks) {
    EXPECT_EQ(editor::core::add(2, 3), 5);
    EXPECT_EQ(editor::core::add(-1, 1), 0);
    EXPECT_EQ(editor::core::add(0, 0), 0);
}

TEST(CoreSanity, VersionIsNonEmpty) {
    EXPECT_NE(editor::core::version(), nullptr);
    EXPECT_NE(editor::core::version()[0], '\0');
}