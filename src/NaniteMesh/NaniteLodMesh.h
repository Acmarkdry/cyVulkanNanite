#pragma once
#include <memory>
#include <vulkan/vulkan_core.h>

#include "Cluster.h"
#include "ClusterGroup.h"
#include "Const.h"
#include "NaniteMesh.h"

class VulkanExampleBase;

namespace vks
{
	struct VulkanDevice;
}

namespace Nanite
{
	class Cluster;
	class ClusterGroup;
    class NaniteBVHNode;

    class NaniteLodMesh
    {
    public:
        int clusterNum;
        NaniteTriMesh mesh;
    	Graph triangleGraph;	
    	std::vector<bool> isEdgeVertices;
    	std::vector<bool> islastLodEdgeVertices;
    	OpenMesh::HPropHandleT<int32_t> clusterGroupIndexPropHandle;
    	std::vector<idx_t> triangleClusterIndex;
    	std::vector<idx_t> clusterGroupIndex;
    	
    	std::vector<uint32_t> triangleIndicesSortedByClusterIdx; // face_idx sort by cluster
    	std::vector<uint32_t> triangleVertexIndicesSortedByClusterIdx;
    	
    	std::vector<Cluster> clusters;
    	
    	int lodLevel;
    	
        // cluster构建
        void buildTriangleGraph();
        void generateCluster();
        
        void getBoundingSphere(Cluster& cluster);
        void calcSurfaceArea(Cluster& cluster);
        
        void createSortedIndexBuffer(VulkanExampleBase& link);
        
        void initUniqueVertexBuffer();
    	
    	vks::Buffer sortedIndices;
    	std::vector<vkglTF::Vertex> uniqueVertexBuffer;

        // mesh lod生成
        std::vector<ClusterGroup> clusterGroups;
        
        void assignTriangleClusterGroup(NaniteLodMesh& lastLoad);
        
        // cluster group构建
        std::vector<ClusterGroup> oldClusterGroups;
    	std::unordered_map<int, int> clusterColorAssignment;
    	Graph clusterGraph;
    	
    	void generateClusterGroup();
    	
    	void simplifyMesh(NaniteTriMesh& mesh);
    	
    	void buildClusterGraph();
    	void colorClusterGraph();
    	void calcBoundingSphereFromChildren(Cluster& cluster, NaniteLodMesh& lastLOD);
        
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
