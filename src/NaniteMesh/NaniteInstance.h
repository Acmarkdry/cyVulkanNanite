#pragma once
#include <vector>

#include "Const.h"
#include "VulkanglTFModel.h"
#include "glm/glm.hpp"

class VulkanExampleBase;

namespace Nanite
{
	class NaniteLodMesh;
	class ClusterInfo;
	class NaniteMesh;

	class NaniteInstance
	{
	public:
		NaniteMesh* referenceMesh = nullptr;
		glm::mat4 rootTransform{1.0f};
		std::vector<ClusterInfo> clusterInfo;
		std::vector<ErrorInfo> errorInfo;
		vkglTF::Model::Vertices vertices;
		vkglTF::Model::Indices indices;
		std::vector<vkglTF::Vertex> vertexBuffer;
		std::vector<uint32_t> indexBuffer;

		NaniteInstance() = default;

		NaniteInstance(NaniteMesh* mesh, const glm::mat4& model)
			: referenceMesh(mesh), rootTransform(model)
		{
		}

		void initBufferForNaniteLODs();
		void buildClusterInfo();

	private:
		[[nodiscard]] glm::vec3 transformPoint(const glm::vec3& point) const;
		[[nodiscard]] glm::vec3 openMeshPointToGlm(const NaniteTriMesh::Point& point) const;
		[[nodiscard]] float calculateWorldRadius(float localRadius) const;
		void processFaceAABB(const NaniteLodMesh& lodMesh, size_t currClusterNum);
		void processClusterIndices(const NaniteLodMesh& lodMesh, size_t currClusterNum, size_t currTriangleNum);
		void processClusterErrors(const NaniteLodMesh& lodMesh, size_t meshIndex, size_t currClusterNum);
	};
}
