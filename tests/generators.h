#pragma once

#include <rapidcheck.h>
#include <glm/glm.hpp>

#include "Cluster.h"
#include "NaniteBVH.h"
#include "NaniteLodMesh.h"

// ── Helper: bounded float generator ─────────────────────────
namespace rc_gen_detail {

inline rc::Gen<float> boundedFloat() {
    return rc::gen::map(rc::gen::inRange(-1000000, 1000001), [](int v) {
        return static_cast<float>(v);
    });
}

inline rc::Gen<double> boundedDouble() {
    return rc::gen::map(rc::gen::inRange(-1000000, 1000001), [](int v) {
        return static_cast<double>(v);
    });
}

} // namespace rc_gen_detail

// ── glm::vec3 ───────────────────────────────────────────────
namespace rc {

template <>
struct Arbitrary<glm::vec3> {
    static Gen<glm::vec3> arbitrary() {
        return gen::map(
            gen::tuple(
                rc_gen_detail::boundedFloat(),
                rc_gen_detail::boundedFloat(),
                rc_gen_detail::boundedFloat()),
            [](const std::tuple<float, float, float>& t) {
                return glm::vec3(std::get<0>(t), std::get<1>(t), std::get<2>(t));
            });
    }
};

// ── glm::vec4 ───────────────────────────────────────────────
template <>
struct Arbitrary<glm::vec4> {
    static Gen<glm::vec4> arbitrary() {
        return gen::map(
            gen::tuple(
                rc_gen_detail::boundedFloat(),
                rc_gen_detail::boundedFloat(),
                rc_gen_detail::boundedFloat(),
                rc_gen_detail::boundedFloat()),
            [](const std::tuple<float, float, float, float>& t) {
                return glm::vec4(std::get<0>(t), std::get<1>(t),
                                 std::get<2>(t), std::get<3>(t));
            });
    }
};

// ── Nanite::Cluster ─────────────────────────────────────────
// Generates only serialization-relevant fields:
//   normalizedlodError, parentNormalizedError, lodError,
//   boundingSphereCenter, boundingSphereRadius,
//   parentClusterIndices, triangleIndices
template <>
struct Arbitrary<Nanite::Cluster> {
    static Gen<Nanite::Cluster> arbitrary() {
        return gen::build<Nanite::Cluster>(
            gen::set(&Nanite::Cluster::normalizedlodError,
                     rc_gen_detail::boundedDouble()),
            gen::set(&Nanite::Cluster::parentNormalizedError,
                     rc_gen_detail::boundedDouble()),
            gen::set(&Nanite::Cluster::lodError,
                     rc_gen_detail::boundedDouble()),
            gen::set(&Nanite::Cluster::boundingSphereCenter,
                     gen::arbitrary<glm::vec3>()),
            gen::set(&Nanite::Cluster::boundingSphereRadius,
                     rc_gen_detail::boundedFloat()),
            gen::set(&Nanite::Cluster::parentClusterIndices,
                     gen::container<std::vector<uint32_t>>(
                         gen::inRange(0u, 128u),
                         gen::inRange(0u, 10000u))),
            gen::set(&Nanite::Cluster::triangleIndices,
                     gen::container<std::vector<uint32_t>>(
                         gen::inRange(0u, 128u),
                         gen::inRange(0u, 10000u))));
    }
};

// ── Nanite::ClusterNode ─────────────────────────────────────
template <>
struct Arbitrary<Nanite::ClusterNode> {
    static Gen<Nanite::ClusterNode> arbitrary() {
        return gen::build<Nanite::ClusterNode>(
            gen::set(&Nanite::ClusterNode::parentMaxLODError,
                     rc_gen_detail::boundedDouble()),
            gen::set(&Nanite::ClusterNode::lodError,
                     rc_gen_detail::boundedDouble()),
            gen::set(&Nanite::ClusterNode::boundingSphereCenter,
                     gen::arbitrary<glm::vec3>()),
            gen::set(&Nanite::ClusterNode::boundingSphereRadius,
                     rc_gen_detail::boundedFloat()));
    }
};

// ── Nanite::NaniteBVHNodeInfo ───────────────────────────────
template <>
struct Arbitrary<Nanite::NaniteBVHNodeInfo> {
    static Gen<Nanite::NaniteBVHNodeInfo> arbitrary() {
        return gen::exec([] {
            Nanite::NaniteBVHNodeInfo info;

            info.normalizedlodError = *rc_gen_detail::boundedDouble();
            info.parentNormalizedError = *rc_gen_detail::boundedDouble();
            info.parentBoundingSphere = *gen::arbitrary<glm::vec4>();
            info.index = *gen::inRange(-1000, 1000);
            info.pMin = *gen::arbitrary<glm::vec3>();
            info.pMax = *gen::arbitrary<glm::vec3>();

            // children: [0, 16] elements
            auto childCount = *gen::inRange(0, 17);
            info.children.resize(childCount);
            for (int i = 0; i < childCount; ++i)
                info.children[i] = *gen::inRange(-1, 1000);

            // clusterIndices: fixed-size array of CLUSTER_GROUP_MAX_SIZE
            for (size_t i = 0; i < Nanite::CLUSTER_GROUP_MAX_SIZE; ++i)
                info.clusterIndices[i] = *gen::inRange(-1, 1000);

            info.start = *gen::inRange(-1, 1000);
            info.end = *gen::inRange(-1, 1000);

            // nodeStatus: random from valid enum values
            info.nodeStatus = *gen::element(
                Nanite::INVALID,
                Nanite::VIRTUAL_NODE,
                Nanite::NODE,
                Nanite::LEAF);

            info.depth = *gen::inRange(0u, 100u);
            info.lodLevel = *gen::inRange(-1, 100);

            return info;
        });
    }
};

// ── Nanite::NaniteLodMesh ───────────────────────────────────
// Only serialization-relevant fields; clusters.size() == clusterNum
template <>
struct Arbitrary<Nanite::NaniteLodMesh> {
    static Gen<Nanite::NaniteLodMesh> arbitrary() {
        return gen::exec([] {
            Nanite::NaniteLodMesh mesh;

            // clusterNum in [0, 8] to keep generation fast
            mesh.clusterNum = *gen::inRange(0, 9);

            // triangleClusterIndex: small vector of idx_t
            auto tciLen = *gen::inRange(0, 64);
            mesh.triangleClusterIndex.resize(tciLen);
            for (int i = 0; i < tciLen; ++i)
                mesh.triangleClusterIndex[i] =
                    static_cast<idx_t>(*gen::inRange(0, mesh.clusterNum > 0 ? mesh.clusterNum : 1));

            // clusterColorAssignment: map from cluster index to color
            for (int i = 0; i < mesh.clusterNum; ++i)
                mesh.clusterColorAssignment[i] = *gen::inRange(0, 8);

            // clusterGroupIndex: small vector of idx_t
            auto cgiLen = *gen::inRange(0, 32);
            mesh.clusterGroupIndex.resize(cgiLen);
            for (int i = 0; i < cgiLen; ++i)
                mesh.clusterGroupIndex[i] = static_cast<idx_t>(*gen::inRange(0, 10));

            // triangleIndicesSortedByClusterIdx
            auto tiLen = *gen::inRange(0, 128);
            mesh.triangleIndicesSortedByClusterIdx.resize(tiLen);
            for (int i = 0; i < tiLen; ++i)
                mesh.triangleIndicesSortedByClusterIdx[i] = *gen::inRange(0u, 10000u);

            // triangleVertexIndicesSortedByClusterIdx
            auto tviLen = *gen::inRange(0, 128);
            mesh.triangleVertexIndicesSortedByClusterIdx.resize(tviLen);
            for (int i = 0; i < tviLen; ++i)
                mesh.triangleVertexIndicesSortedByClusterIdx[i] = *gen::inRange(0u, 10000u);

            // clusters: count must match clusterNum
            mesh.clusters.resize(mesh.clusterNum);
            for (int i = 0; i < mesh.clusterNum; ++i)
                mesh.clusters[i] = *gen::arbitrary<Nanite::Cluster>();

            return mesh;
        });
    }
};

} // namespace rc
