#include <gtest/gtest.h>
#include "Engine/Core/Reflection/TypeRegistry.h"
#include "Engine/Core/Reflection/EnumRegistry.h"
#include "Engine/Core/DataModel/DataModel.h"
#include "Engine/Core/DataModel/Part.h"

using namespace Engine::Reflection;

class ReflectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Finalize registry before running any test
        static bool initialized = false;
        if (!initialized) {
            TypeRegistry::instance().finalize();
            initialized = true;
        }
    }
};

TEST_F(ReflectionTest, FindClassDescriptors) {
    auto* desc = TypeRegistry::instance().find("Part");
    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc->className, "Part");
}

TEST_F(ReflectionTest, InheritanceIsA) {
    auto* partDesc = TypeRegistry::instance().find("Part");
    ASSERT_NE(partDesc, nullptr);
    
    EXPECT_TRUE(partDesc->isA("Part"));
    EXPECT_TRUE(partDesc->isA("Instance"));
    EXPECT_FALSE(partDesc->isA("DataModel"));
}

TEST_F(ReflectionTest, InstanceFactory) {
    auto instance = createInstance("Part");
    ASSERT_NE(instance, nullptr);
    EXPECT_EQ(instance->name, "Instance"); // default name
}

TEST_F(ReflectionTest, Properties) {
    auto* partDesc = TypeRegistry::instance().find("Part");
    ASSERT_NE(partDesc, nullptr);

    auto* posProp = partDesc->findProperty("Position");
    ASSERT_NE(posProp, nullptr);
    EXPECT_EQ(posProp->kind, PropertyDescriptor::Kind::Primitive);

    auto instance = createInstance("Part");
    auto part = std::dynamic_pointer_cast<Part>(instance);
    ASSERT_NE(part, nullptr);

    // Initial value
    auto posVal = posProp->getter(part.get());
    auto vec = std::any_cast<Engine::Math::Vector3>(posVal);
    EXPECT_FLOAT_EQ(vec.x, 0.0f);

    // Set value
    posProp->setter(part.get(), std::any(Engine::Math::Vector3(1.0f, 2.0f, 3.0f)));
    EXPECT_FLOAT_EQ(part->position.x, 1.0f);
    EXPECT_FLOAT_EQ(part->position.y, 2.0f);
    EXPECT_FLOAT_EQ(part->position.z, 3.0f);
}

TEST_F(ReflectionTest, EnumProperty) {
    auto* partDesc = TypeRegistry::instance().find("Part");
    ASSERT_NE(partDesc, nullptr);

    auto* matProp = partDesc->findProperty("Material");
    ASSERT_NE(matProp, nullptr);
    EXPECT_EQ(matProp->kind, PropertyDescriptor::Kind::Enum);
    EXPECT_EQ(matProp->enumTypeName, "Material");

    auto instance = createInstance("Part");
    auto part = std::dynamic_pointer_cast<Part>(instance);
    ASSERT_NE(part, nullptr);

    // Check enum registry
    auto* enumDesc = EnumRegistry::instance().find("Material");
    ASSERT_NE(enumDesc, nullptr);
    EXPECT_EQ(enumDesc->nameOf(static_cast<int>(Material::Wood)), "Wood");

    // Getter
    auto val = matProp->getter(part.get());
    EXPECT_EQ(std::any_cast<int>(val), static_cast<int>(Material::Plastic)); // Default value

    // Setter
    matProp->setter(part.get(), std::any(static_cast<int>(Material::Wood)));
    EXPECT_EQ(part->material, Material::Wood);
}

TEST_F(ReflectionTest, HierarchyAndWeakPtr) {
    auto root = createInstance("DataModel");
    auto part1 = createInstance("Part");
    
    part1->setParent(root);
    
    auto children = root->getChildren();
    ASSERT_EQ(children.size(), 1);
    EXPECT_EQ(children[0], part1);
    
    EXPECT_EQ(part1->getParent(), root);
    
    part1->destroy();
    
    children = root->getChildren();
    EXPECT_EQ(children.size(), 0);
    EXPECT_EQ(part1->getParent(), nullptr);
}
