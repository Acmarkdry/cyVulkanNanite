// tests/test_cluster_json.cpp
// Cluster 与 ClusterNode JSON 序列化测试
// 验证需求 6.1–6.4

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include "Cluster.h"
#include "generators.h"

// ============================================================
// 7.1  属性测试：Cluster JSON 序列化往返（属性 7）
// Feature: nanite-testing, Property 7: Cluster JSON 序列化往返
// **Validates: Requirements 6.1, 6.4**
//
// 对于任意有效的 Cluster 对象，执行 toJson() 后再 fromJson()
// 应产生与原始对象等价的 Cluster。
// 注意：boundingSphereCenter 在序列化时为 float，使用近似比较。
// ============================================================
RC_GTEST_PROP(ClusterJsonTest, RoundTrip, ())
{
    auto original = *rc::gen::arbitrary<Nanite::Cluster>();

    auto json = original.toJson();

    Nanite::Cluster restored;
    restored.fromJson(json);

    // double 字段：精确比较
    RC_ASSERT(original.normalizedlodError == restored.normalizedlodError);
    RC_ASSERT(original.parentNormalizedError == restored.parentNormalizedError);
    RC_ASSERT(original.lodError == restored.lodError);

    // boundingSphereCenter: float 精度，近似比较
    RC_ASSERT(original.boundingSphereCenter.x == restored.boundingSphereCenter.x);
    RC_ASSERT(original.boundingSphereCenter.y == restored.boundingSphereCenter.y);
    RC_ASSERT(original.boundingSphereCenter.z == restored.boundingSphereCenter.z);

    // boundingSphereRadius: 序列化为 double 再读回 double，但原始为 float
    // toJson 输出 float → json number → fromJson get<double>
    // 使用近似比较以处理 float→double 精度差异
    const double radiusEps = 1e-6;
    RC_ASSERT(std::abs(static_cast<double>(original.boundingSphereRadius) -
                       restored.boundingSphereRadius) < radiusEps);

    // vector 字段：精确比较
    RC_ASSERT(original.parentClusterIndices == restored.parentClusterIndices);
    RC_ASSERT(original.triangleIndices == restored.triangleIndices);
}

// ============================================================
// 7.2  属性测试：ClusterNode JSON 序列化往返（属性 8）
// Feature: nanite-testing, Property 8: ClusterNode JSON 序列化往返
// **Validates: Requirements 6.2**
//
// 对于任意有效的 ClusterNode 对象，执行 toJson() 后再 fromJson()
// 应产生与原始对象等价的 ClusterNode。
// ============================================================
RC_GTEST_PROP(ClusterNodeJsonTest, RoundTrip, ())
{
    auto original = *rc::gen::arbitrary<Nanite::ClusterNode>();

    auto json = original.toJson();

    Nanite::ClusterNode restored;
    restored.fromJson(json);

    // double 字段：精确比较
    RC_ASSERT(original.parentMaxLODError == restored.parentMaxLODError);
    RC_ASSERT(original.lodError == restored.lodError);

    // boundingSphereCenter: float 精度
    RC_ASSERT(original.boundingSphereCenter.x == restored.boundingSphereCenter.x);
    RC_ASSERT(original.boundingSphereCenter.y == restored.boundingSphereCenter.y);
    RC_ASSERT(original.boundingSphereCenter.z == restored.boundingSphereCenter.z);

    // boundingSphereRadius: float→double 近似比较
    const double radiusEps = 1e-6;
    RC_ASSERT(std::abs(static_cast<double>(original.boundingSphereRadius) -
                       restored.boundingSphereRadius) < radiusEps);
}

// ============================================================
// 7.3  单元测试：Cluster::fromJson 缺少字段时触发断言
// **Validates: Requirements 6.3**
//
// 当 fromJson() 接收到缺少必要字段的 JSON 数据时，
// Cluster 应触发 NaniteAssert（调用 std::abort）。
// ============================================================
TEST(ClusterJsonDeathTest, FromJsonMissingFieldsTriggersAssert)
{
    Nanite::Cluster cluster;
    nlohmann::json emptyJson = nlohmann::json::object();

    EXPECT_DEATH(cluster.fromJson(emptyJson), ".*");
}
