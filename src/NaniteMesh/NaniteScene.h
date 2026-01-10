#pragma once
#include "NaniteInstance.h"
#include "NaniteMesh.h"


namespace Nanite
{
	class NaniteScene
	{
	public:
		std::vector<NaniteInstance> naniteObjects;
		std::vector<NaniteMesh> naniteMeshes;
		std::map<std::string, uint32_t> naniteMeshSceneIndices;
		std::vector<uint32_t> indexOffsets;
		std::vector<uint32_t> indexCounts;

		vkglTF::Model::Vertices vertices;
		vkglTF::Model::Indices indices;

		uint32_t sceneIndicesCount = 0;
		uint32_t visibleIndicesCount = 0;

		std::vector<ClusterInfo> clusterInfo;
		std::vector<ErrorInfo> errorInfo;

		void createVertexIndexBuffer(vks::VulkanDevice* device, VkQueue transferQueue, VulkanExampleBase& link);
		void createClusterInfos();
	};
}
