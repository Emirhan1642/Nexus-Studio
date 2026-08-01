#include <gtest/gtest.h>
#include "Engine/Core/Math/Vector3.h"

// Note: A placeholder for actual Matrix/Transform math tests.
// In actual implementations, this would test coordinate system conversions.

using namespace Engine::Math;

TEST(TransformTests, Translation) {
    Vector3 pos(1.0f, 1.0f, 1.0f);
    Vector3 delta(2.0f, 0.0f, -1.0f);
    
    Vector3 result = pos + delta;
    EXPECT_FLOAT_EQ(result.x, 3.0f);
    EXPECT_FLOAT_EQ(result.y, 1.0f);
    EXPECT_FLOAT_EQ(result.z, 0.0f);
}
