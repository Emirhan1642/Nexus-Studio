#include <gtest/gtest.h>
#include "Engine/Core/Math/Vector3.h"

using namespace Engine::Math;

TEST(MathTests, Vector3Basic) {
    Vector3 v1(1.0f, 2.0f, 3.0f);
    Vector3 v2(4.0f, 5.0f, 6.0f);

    Vector3 add = v1 + v2;
    EXPECT_FLOAT_EQ(add.x, 5.0f);
    EXPECT_FLOAT_EQ(add.y, 7.0f);
    EXPECT_FLOAT_EQ(add.z, 9.0f);

    Vector3 sub = v2 - v1;
    EXPECT_FLOAT_EQ(sub.x, 3.0f);
    EXPECT_FLOAT_EQ(sub.y, 3.0f);
    EXPECT_FLOAT_EQ(sub.z, 3.0f);

    Vector3 mul = v1 * 2.0f;
    EXPECT_FLOAT_EQ(mul.x, 2.0f);
    EXPECT_FLOAT_EQ(mul.y, 4.0f);
    EXPECT_FLOAT_EQ(mul.z, 6.0f);
}
