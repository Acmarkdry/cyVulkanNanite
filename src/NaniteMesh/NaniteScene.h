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
		uint32_t visibleIndicesCount = 0;

		void createVertexIndexBuffer(VulkanExampleBase& link);
		void createClusterInfos();

	private:
		[[nodiscard]] size_t calculateTotalVertexCount() const;
		[[nodiscard]] size_t calculateTotalIndexCount() const;
		[[nodiscard]] ptrdiff_t findMeshIndex(const NaniteMesh& mesh) const;
	};
}