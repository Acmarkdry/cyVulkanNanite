// tests/test_lodmesh_json.cpp
// NaniteLodMesh JSON 序列化测试
// 验证需求 8.1–8.3

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include "NaniteLodMesh.h"
#include "generators.h"

// ============================================================
// 9.1  属性测试：NaniteLodMesh JSON 序列化往返（属性 10）
// Feature: nanite-testing, Property 10: NaniteLodMesh JSON 序列化往返
// **Validates: Requirements 8.1, 8.2, 8.3**
//
// 对于任意有效的 NaniteLodMesh 对象（仅包含序列化字段），
// 执行 toJson() 后再 fromJson() 应产生与原始对象等价的
// NaniteLodMesh，且反序列化后的 clusters.size() 等于 clusterNum。
// ============================================================
RC_GTEST_PROP(LodMeshJsonTest, RoundTrip, ())
{
    auto original = *rc::gen::arbitrary<Nanite::NaniteLodMesh>();

    auto json = original.toJson();

    Nanite::NaniteLodMesh restored;
    restored.fromJson(json);

    // clusterNum
    RC_ASSERT(original.clusterNum == restored.clusterNum);

    // clusters.size() == clusterNum (需求 8.3)
    RC_ASSERT(static_cast<int>(restored.clusters.size()) == restored.clusterNum);

    // triangleClusterIndex
    RC_ASSERT(original.triangleClusterIndex == restored.triangleClusterIndex);

    // clusterColorAssignment
    RC_ASSERT(original.clusterColorAssignment == restored.clusterColorAssignment);

    // clusterGroupIndex
    RC_ASSERT(original.clusterGroupIndex == restored.clusterGroupIndex);

    // triangleIndicesSortedByClusterIdx
    RC_ASSERT(original.triangleIndicesSortedByClusterIdx ==
              restored.triangleIndicesSortedByClusterIdx);

    // triangleVertexIndicesSortedByClusterIdx
    RC_ASSERT(original.triangleVertexIndicesSortedByClusterIdx ==
              restored.triangleVertexIndicesSortedByClusterIdx);

    // 逐个验证 clusters 往返
    RC_ASSERT(original.clusters.size() == restored.clusters.size());
    for (size_t i = 0; i < original.clusters.size(); ++i) {
        const auto& oc = original.clusters[i];
        const auto& rc = restored.clusters[i];

        RC_ASSERT(oc.normalizedlodError == rc.normalizedlodError);
        RC_ASSERT(oc.parentNormalizedError == rc.parentNormalizedError);
        RC_ASSERT(oc.lodError == rc.lodError);

        RC_ASSERT(oc.boundingSphereCenter.x == rc.boundingSphereCenter.x);
        RC_ASSERT(oc.boundingSphereCenter.y == rc.boundingSphereCenter.y);
        RC_ASSERT(oc.boundingSphereCenter.z == rc.boundingSphereCenter.z);
        RC_ASSERT(oc.boundingSphereRadius == rc.boundingSphereRadius);

        RC_ASSERT(oc.parentClusterIndices == rc.parentClusterIndices);
        RC_ASSERT(oc.triangleIndices == rc.triangleIndices);
    }
}
