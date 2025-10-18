#pragma once
#include <vector>
#include <vulkan/vulkan.h>

#include "Const.h"
#include "NaniteInstance.h"
#include "NaniteMesh.h"
#include "VulkanglTFModel.h"

namespace vks
{
	struct VulkanDevice;
}

class VulkanExampleBase;

namespace Nanite
{
	class NaniteScene
	{
	public:
		std::vector<NaniteMesh> naniteMeshes;
		std::vector<NaniteInstance> naniteObjects;

		vkglTF::Model::Vertices vertices;
		vkglTF::Model::Indices indices;

		std::vector<uint32_t> indexOffsets;
		std::vector<uint32_t> indexCounts;

		std::vector<ClusterInfo> clusterInfo;
		std::vector<ErrorInfo> errorInfo;

		uint32_t sceneIndicesCount = 0;

		std::shared_ptr<NaniteBVHNode> virtualRootNode; // The root node that connects all instances' root nodes
		std::vector<BVHNodeInfo> nodeInfos; // cleaned version
		std::vector<uint32_t> clusterIndexOffsets; 
		std::vector<BVHNodeInfo> bvhNodeInfos; // cleaned version
		std::vector<uint32_t> depthCounts;
		std::vector<uint32_t> depthLeafCounts; // Just for stats, not in usage
		std::vector<uint32_t> initNodeInfoIndices;
		uint32_t maxDepthCounts = 0;

		std::vector<uint32_t> clusterIndexCounts;
		uint32_t maxClusterNum = 0;

		std::vector<uint32_t> sortedClusterIndices;

		uint32_t maxLodLevelNum = -1;
		
		void createNaniteSceneInfo(VulkanExampleBase& link);
		void createVertexIndexBuffer(VulkanExampleBase& link);
		void createClusterInfos(VulkanExampleBase& link);
		void createBVHNodeInfos(VulkanExampleBase& link);
	
	private:
		[[nodiscard]] size_t calculateTotalVertexCount() const;
		[[nodiscard]] size_t calculateTotalIndexCount() const;
		[[nodiscard]] ptrdiff_t findMeshIndex(const NaniteMesh& mesh) const;
	};
}