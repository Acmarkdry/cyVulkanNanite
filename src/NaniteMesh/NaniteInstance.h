#pragma once
#include <vector>

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
		vks::Buffer vertices;
		vks::Buffer indices;
		
		NaniteInstance() = default;
		NaniteInstance(NaniteMesh* mesh, const glm::mat4 model):referenceMesh(mesh), rootTransform(model){}
		
		void createBuffersForNaniteLod(VulkanExampleBase &link);
		void buildClusterInfo();
	};
}
