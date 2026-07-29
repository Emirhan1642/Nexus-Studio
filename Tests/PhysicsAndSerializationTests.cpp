#include <gtest/gtest.h>
#include "Engine/Core/Reflection/TypeRegistry.h"
#include "Engine/Core/DataModel/DataModelSerializer.h"
#include "Engine/Core/DataModel/HingeConstraint.h"
#include "Engine/Core/DataModel/Part.h"
#include "Engine/Physics/PhysicsWorld.h"

#include "TestEnvironment.h"

using namespace Engine::Reflection;
using namespace Engine::Core;

class PhysicsAndSerializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        ensureTestEnvironmentInitialized();
    }
};

TEST_F(PhysicsAndSerializationTest, ObjectRefSerialization) {
    auto root = createInstance("DataModel");
    root->name = "Workspace";

    auto part0 = createInstance("Part");
    part0->name = "Part0";
    part0->setParent(root);

    auto part1 = createInstance("Part");
    part1->name = "Part1";
    part1->setParent(root);

    auto hinge = createInstance("HingeConstraint");
    hinge->name = "Hinge";
    hinge->setParent(part0);

    auto* hingeDesc = TypeRegistry::instance().find("HingeConstraint");
    ASSERT_NE(hingeDesc, nullptr);
    
    auto* p0Prop = hingeDesc->findProperty("Part0");
    auto* p1Prop = hingeDesc->findProperty("Part1");
    
    ASSERT_NE(p0Prop, nullptr);
    ASSERT_NE(p1Prop, nullptr);

    p0Prop->objectSetter(hinge.get(), part0);
    p1Prop->objectSetter(hinge.get(), part1);

    auto j = DataModelSerializer::serialize(root);
    
    auto deserializedRoot = DataModelSerializer::deserialize(j);
    ASSERT_NE(deserializedRoot, nullptr);
    
    auto dPart0 = deserializedRoot->findFirstChild("Part0");
    ASSERT_NE(dPart0, nullptr);
    
    auto dHinge = dPart0->findFirstChild("Hinge");
    ASSERT_NE(dHinge, nullptr);
    
    auto dPart0Ref = p0Prop->objectGetter(dHinge.get());
    auto dPart1Ref = p1Prop->objectGetter(dHinge.get());
    
    ASSERT_NE(dPart0Ref, nullptr);
    ASSERT_NE(dPart1Ref, nullptr);
    
    EXPECT_EQ(dPart0Ref->name, "Part0");
    EXPECT_EQ(dPart1Ref->name, "Part1");

    // Clean up to prevent Jolt assertions on exit
    hinge->destroy();
    part1->destroy();
    part0->destroy();
    root->destroy();

    dHinge->destroy();
    dPart1Ref->destroy();
    dPart0Ref->destroy();
    deserializedRoot->destroy();
}
