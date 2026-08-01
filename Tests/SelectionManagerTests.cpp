#include <gtest/gtest.h>
#include "Editor/UI/SelectionManager.h"
#include "Engine/Core/DataModel/Part.h"

using namespace Engine::Reflection;

class SelectionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        SelectionManager::instance().clear();
    }
    
    void TearDown() override {
        SelectionManager::instance().clear();
    }
};

TEST_F(SelectionManagerTest, SelectAndClear) {
    auto part = createInstance("Part");
    ASSERT_NE(part, nullptr);

    SelectionManager::instance().select(part);
    auto selected = SelectionManager::instance().getSelected();
    EXPECT_EQ(selected, part);

    SelectionManager::instance().clear();
    selected = SelectionManager::instance().getSelected();
    EXPECT_EQ(selected, nullptr);
}

TEST_F(SelectionManagerTest, ChangeSelection) {
    auto part1 = createInstance("Part");
    auto part2 = createInstance("Part");

    SelectionManager::instance().select(part1);
    EXPECT_EQ(SelectionManager::instance().getSelected(), part1);

    SelectionManager::instance().select(part2);
    EXPECT_EQ(SelectionManager::instance().getSelected(), part2);
}
