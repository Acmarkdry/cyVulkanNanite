// tests/test_bvh_json.cpp
// NaniteBVHNodeInfo JSON 序列化测试
// 验证需求 7.1–7.3

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include "NaniteBVH.h"
#include "generators.h"

// ============================================================
// 8.1  属性测试：NaniteBVHNodeInfo JSON 序列化往返（属性 9）
// Feature: nanite-testing, Property 9: NaniteBVHNodeInfo JSON 序列化往返
// **Validates: Requirements 7.1, 7.3**
//
// 对于任意有效的 NaniteBVHNodeInfo 对象，执行 toJson() 后再
// fromJson() 应产生与原始对象等价的 NaniteBVHNodeInfo，
// 包括所有 CLUSTER_GROUP_MAX_SIZE 个 clusterIndices 元素。
// ============================================================
RC_GTEST_PROP(BvhJsonTest, RoundTrip, ())
{
    auto original = *rc::gen::arbitrary<Nanite::NaniteBVHNodeInfo>();

    auto json = original.toJson();

    Nanite::NaniteBVHNodeInfo restored;
    restored.fromJson(json);

    // double 字段：精确比较
    RC_ASSERT(original.normalizedlodError == restored.normalizedlodError);
    RC_ASSERT(original.parentNormalizedError == restored.parentNormalizedError);

    // parentBoundingSphere: float 精度
    RC_ASSERT(original.parentBoundingSphere.x == restored.parentBoundingSphere.x);
    RC_ASSERT(original.parentBoundingSphere.y == restored.parentBoundingSphere.y);
    RC_ASSERT(original.parentBoundingSphere.z == restored.parentBoundingSphere.z);
    RC_ASSERT(original.parentBoundingSphere.w == restored.parentBoundingSphere.w);

    // int 字段
    RC_ASSERT(original.index == restored.index);

    // pMin / pMax: float 精度
    RC_ASSERT(original.pMin.x == restored.pMin.x);
    RC_ASSERT(original.pMin.y == restored.pMin.y);
    RC_ASSERT(original.pMin.z == restored.pMin.z);
    RC_ASSERT(original.pMax.x == restored.pMax.x);
    RC_ASSERT(original.pMax.y == restored.pMax.y);
    RC_ASSERT(original.pMax.z == restored.pMax.z);

    // children vector
    RC_ASSERT(original.children == restored.children);

    // clusterIndices: 验证所有 CLUSTER_GROUP_MAX_SIZE 个元素
    for (size_t i = 0; i < Nanite::CLUSTER_GROUP_MAX_SIZE; ++i) {
        RC_ASSERT(original.clusterIndices[i] == restored.clusterIndices[i]);
    }

    // 其余标量字段
    RC_ASSERT(original.start == restored.start);
    RC_ASSERT(original.end == restored.end);
    RC_ASSERT(original.nodeStatus == restored.nodeStatus);
    RC_ASSERT(original.depth == restored.depth);
    RC_ASSERT(original.lodLevel == restored.lodLevel);
}

// ============================================================
// 8.2  单元测试：NaniteBVHNodeInfo::fromJson 缺少字段时触发断言
// **Validates: Requirements 7.2**
//
// 当 fromJson() 接收到缺少必要字段的 JSON 数据时，
// NaniteBVHNodeInfo 应触发 NaniteAssert（调用 std::abort）。
// ============================================================
TEST(BvhJsonDeathTest, FromJsonMissingFieldsTriggersAssert)
{
    Nanite::NaniteBVHNodeInfo info;
    nlohmann::json emptyJson = nlohmann::json::object();

    EXPECT_DEATH(info.fromJson(emptyJson), ".*");
}
