#pragma once
#include <memory>
#include <glm/detail/type_vec.hpp>

#include "Const.h"

namespace Nanite
{
    class ClusterGroup;
    class NaniteBVHNode;

    class NaniteLodMesh
    {
    public:
        int clusterNum;
        NaniteTriMesh mesh;
        
        // bvh 构建相关
        void createBVH();
        void buildBVH();
        void updateBVHError();
        void updateBVHErrorCore(std::shared_ptr<NaniteBVHNode> currNode, float& currNodeError, glm::vec4& currNodeBoundingSphere);
        void traverseBVH();
        void getClusterGroupAABB(ClusterGroup & clusterGroup);
        void flattenBVH();
    };
}
