// tests/test_geometry.cpp
// 几何工具函数测试 — getTriangleAABB
// 验证需求 9.1–9.4

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <glm/glm.hpp>

#include "utils.h"
#include "generators.h"

// ============================================================
// 10.1  属性测试：getTriangleAABB 计算正确性（属性 11）
// Feature: nanite-testing, Property 11: getTriangleAABB 计算正确性
// **Validates: Requirements 9.1, 9.2, 9.3**
//
// 对于任意三个 3D 顶点 p0、p1、p2，getTriangleAABB 计算出的
// pMin 的每个分量应等于三个顶点对应分量的最小值，
// pMax 的每个分量应等于三个顶点对应分量的最大值。
// ============================================================
RC_GTEST_PROP(GeometryPropertyTest, TriangleAABBCorrectness, ())
{
    auto p0 = *rc::gen::arbitrary<glm::vec3>();
    auto p1 = *rc::gen::arbitrary<glm::vec3>();
    auto p2 = *rc::gen::arbitrary<glm::vec3>();

    glm::vec3 pMin, pMax;
    Nanite::getTriangleAABB(p0, p1, p2, pMin, pMax);

    // pMin 每个分量应等于三个顶点对应分量的最小值
    RC_ASSERT(pMin.x == std::min({p0.x, p1.x, p2.x}));
    RC_ASSERT(pMin.y == std::min({p0.y, p1.y, p2.y}));
    RC_ASSERT(pMin.z == std::min({p0.z, p1.z, p2.z}));

    // pMax 每个分量应等于三个顶点对应分量的最大值
    RC_ASSERT(pMax.x == std::max({p0.x, p1.x, p2.x}));
    RC_ASSERT(pMax.y == std::max({p0.y, p1.y, p2.y}));
    RC_ASSERT(pMax.z == std::max({p0.z, p1.z, p2.z}));
}

// ============================================================
// 10.2  单元测试：退化三角形（三个顶点相同）的 AABB 计算
// **Validates: Requirements 9.4**
//
// 当三个顶点相同时，getTriangleAABB 应返回退化的包围盒
// （pMin 等于 pMax 等于该顶点）。
// ============================================================
TEST(GeometryUnitTest, DegenerateTriangleAABB)
{
    const glm::vec3 vertex(3.0f, -7.5f, 42.0f);

    glm::vec3 pMin, pMax;
    Nanite::getTriangleAABB(vertex, vertex, vertex, pMin, pMax);

    EXPECT_EQ(pMin.x, vertex.x);
    EXPECT_EQ(pMin.y, vertex.y);
    EXPECT_EQ(pMin.z, vertex.z);

    EXPECT_EQ(pMax.x, vertex.x);
    EXPECT_EQ(pMax.y, vertex.y);
    EXPECT_EQ(pMax.z, vertex.z);
}
