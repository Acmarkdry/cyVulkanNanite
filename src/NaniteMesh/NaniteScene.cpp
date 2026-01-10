#include "NaniteScene.h"
#include <OpenMesh/Core/IO/MeshIO.hh>

#include "../vksTools.h"

namespace Nanite
{
	void NaniteScene::createVertexIndexBuffer(vks::VulkanDevice* device, VkQueue transferQueue, VulkanExampleBase& link)
	{
		std::vector<vkglTF::Vertex> vertexBuffer;
		std::vector<uint32_t> indexBuffer;

		int indexOffset = 0;
		indexOffsets.resize(naniteMeshes.size());
		indexCounts.resize(naniteMeshes.size());

		for (int i = 0; i < naniteMeshes.size(); ++i)
		{
			auto naniteMesh = naniteMeshes[i];
			auto instance = NaniteInstance(&naniteMesh, glm::mat4(1.0f));
			instance.initBufferForNaniteLODs();
			vertexBuffer.insert(vertexBuffer.end(), instance.vertexBuffer.begin(), instance.vertexBuffer.end());
			for (auto index : instance.indexBuffer)
			{
				index += indexOffset;
				indexBuffer.push_back(index);
			}
			indexOffsets[i] = indexOffset;
			indexCounts[i] = instance.indexBuffer.size();
			indexOffset += instance.vertexBuffer.size();
		}

		size_t vertexBufferSize = vertexBuffer.size() * sizeof(vkglTF::Vertex);
		vertices.count = static_cast<uint32_t>(vertexBuffer.size());
		vks::vksTools::createStagingBuffer(link, 0, vertexBufferSize, vertexBuffer.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,vertices);
		
		size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
		indices.count = static_cast<uint32_t>(indexBuffer.size());
		vks::vksTools::createStagingBuffer(link, 0, indexBufferSize, indexBuffer.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, indices);
	}


	void NaniteScene::createClusterInfos()
	{
		sceneIndicesCount = 0;
		for (int i = 0; i < naniteObjects.size(); ++i)
		{
			auto& naniteObject = naniteObjects[i];
			naniteObject.buildClusterInfo();
			auto referenceMeshIndex = std::find(naniteMeshes.begin(), naniteMeshes.end(), *(naniteObject.referenceMesh)) - naniteMeshes.begin();
			for (auto ci : naniteObject.clusterInfo)
			{
				if (referenceMeshIndex != 0)
				{
					ci.triangleIndicesStart += indexCounts[referenceMeshIndex - 1] / 3;
					ci.triangleIndicesEnd += indexCounts[referenceMeshIndex - 1] / 3;
				}
				ci.objectIdx = i;
				clusterInfo.push_back(ci);
			}
			errorInfo.insert(errorInfo.end(), naniteObject.errorInfo.begin(), naniteObject.errorInfo.end());
			sceneIndicesCount += indexCounts[referenceMeshIndex];
			visibleIndicesCount += naniteObject.referenceMesh->meshes[0].triangleVertexIndicesSortedByClusterIdx.size();
		}
	}
}
