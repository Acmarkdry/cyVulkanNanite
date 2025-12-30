// tests/test_clusterinfo.cpp
// ClusterInfo AABB 合并测试 — mergeAABB 单调包含性
// 验证需求 10.1–10.4

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <glm/glm.hpp>

#include "Const.h"
#include "generators.h"

// Helper: generate a valid AABB pair where pMin <= pMax component-wise
static rc::Gen<std::pair<glm::vec3, glm::vec3>> genAABB()
{
    return rc::gen::map(
        rc::gen::tuple(
            rc::gen::arbitrary<glm::vec3>(),
            rc::gen::arbitrary<glm::vec3>()),
        [](const std::tuple<glm::vec3, glm::vec3>& t) {
            glm::vec3 a = std::get<0>(t);
            glm::vec3 b = std::get<1>(t);
            glm::vec3 pMin = glm::min(a, b);
            glm::vec3 pMax = glm::max(a, b);
            return std::make_pair(pMin, pMax);
        });
}

// ============================================================
// 11.1  属性测试：ClusterInfo::mergeAABB 单调包含性（属性 12）
// Feature: nanite-testing, Property 12: ClusterInfo::mergeAABB 单调包含性
// **Validates: Requirements 10.1, 10.2, 10.3, 10.4**
//
// 对于任意序列的 AABB 合并操作，每次调用 mergeAABB 后：
// 1. pMinWorld 的每个分量应 <= 合并前的值（单调递减）
// 2. pMaxWorld 的每个分量应 >= 合并前的值（单调递增）
// 3. 最终包围盒应包含所有已合并的包围盒
// ============================================================
RC_GTEST_PROP(ClusterInfoPropertyTest, MergeAABBMonotonicity, ())
{
    // Generate 1..20 random AABBs
    auto aabbs = *rc::gen::container<std::vector<std::pair<glm::vec3, glm::vec3>>>(
        rc::gen::inRange(1, 21), genAABB());

    Nanite::ClusterInfo info;
    // Default: pMinWorld = FLT_MAX, pMaxWorld = -FLT_MAX

    for (const auto& [pMin, pMax] : aabbs) {
        glm::vec3 prevMin = info.pMinWorld;
        glm::vec3 prevMax = info.pMaxWorld;

        info.mergeAABB(pMin, pMax);

        // 1. pMinWorld should be <= previous pMinWorld (monotonically non-increasing)
        RC_ASSERT(info.pMinWorld.x <= prevMin.x);
        RC_ASSERT(info.pMinWorld.y <= prevMin.y);
        RC_ASSERT(info.pMinWorld.z <= prevMin.z);

        // 2. pMaxWorld should be >= previous pMaxWorld (monotonically non-decreasing)
        RC_ASSERT(info.pMaxWorld.x >= prevMax.x);
        RC_ASSERT(info.pMaxWorld.y >= prevMax.y);
        RC_ASSERT(info.pMaxWorld.z >= prevMax.z);
    }

    // 3. Final bounding box contains all input AABBs
    for (const auto& [pMin, pMax] : aabbs) {
        RC_ASSERT(info.pMinWorld.x <= pMin.x);
        RC_ASSERT(info.pMinWorld.y <= pMin.y);
        RC_ASSERT(info.pMinWorld.z <= pMin.z);

        RC_ASSERT(info.pMaxWorld.x >= pMax.x);
        RC_ASSERT(info.pMaxWorld.y >= pMax.y);
        RC_ASSERT(info.pMaxWorld.z >= pMax.z);
    }
}
