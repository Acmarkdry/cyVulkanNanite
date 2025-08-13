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
		NaniteMesh* referenceMesh;
		glm::mat4 rootTransform;
		std::vector<ClusterInfo> clusterInfo;
		std::vector<ErrorInfo> errorInfo;
		vkglTF::Model::Vertices vertices;
		vkglTF::Model::Indices indices;
		std::vector<vkglTF::Vertex> vertexBuffer;
		std::vector<uint32_t> indexBuffer;
		NaniteInstance() = default;
		NaniteInstance(NaniteMesh* mesh, const glm::mat4 model):referenceMesh(mesh), rootTransform(model){}
		
		void initBufferForNaniteLODs();
		void buildClusterInfo();
	};
}
