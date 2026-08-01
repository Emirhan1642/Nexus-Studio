#include <gtest/gtest.h>
#include "Engine/Core/DataModel/DataModel.h"
#include "Engine/Core/DataModel/Part.h"

TEST(DataModelTests, InstanceParenting) {
    auto parent = std::make_shared<Instance>();
    parent->name = "Parent";
    auto child = std::make_shared<Instance>();
    child->name = "Child";

    child->setParent(parent);

    EXPECT_EQ(child->getParent(), parent);
    EXPECT_EQ(parent->getChildren().size(), 1);
    EXPECT_EQ(parent->getChildren()[0], child);

    child->setParent(nullptr);

    EXPECT_EQ(child->getParent(), nullptr);
    EXPECT_EQ(parent->getChildren().size(), 0);
}

TEST(DataModelTests, PartProperties) {
    auto part = std::make_shared<Part>();
    EXPECT_EQ(part->getClassName(), "Part");

    part->size = Engine::Math::Vector3(2.0f, 2.0f, 2.0f);
    EXPECT_EQ(part->size.x, 2.0f);
    EXPECT_EQ(part->size.y, 2.0f);
    EXPECT_EQ(part->size.z, 2.0f);
}
