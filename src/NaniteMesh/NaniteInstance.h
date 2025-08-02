#pragma once
#include <vector>

#include "glm/glm.hpp"

namespace Nanite 
{
	class NaniteLodMesh;
	class ClusterInfo;
	class NaniteMesh;

	class NaniteInstance 
	{
	public:
		NaniteLodMesh * referenceMesh;
		glm::mat4 transform;
		std::vector<ClusterInfo> clusterInfos;
		NaniteInstance() = default;
		NaniteInstance(NaniteLodMesh *mesh, const glm::mat4 modelMatrix): referenceMesh(mesh), transform(modelMatrix) {}
		
		void buildClusterInfo();
	};
}
