#pragma once
#include <cstdint>
#include <json.hpp>
#include <vector>
#include <glm/glm.hpp>

namespace Nanite
{
	class Cluster
	{
	public:
		uint32_t clusterGroupIndex;
		std::vector<uint32_t> triangleIndices;
		std::vector<uint32_t> parentClusterIndices;
		std::vector<uint32_t> childClusterIndices;
		double qemError = -1;
		double lodError = -1;
		double normalizedlodError = -1;
		double childLODErrorMax = 0.0;
		double parentNormalizedError = -1;
		bool isLeaf = true;
		uint32_t lodLevel = -1;

		float surfaceArea = 0.0f;
		float parentSurfaceArea = 0.0f;
		glm::vec3 boundingSphereCenter;
		float boundingSphereRadius;
		
		nlohmann::json toJson();
		void fromJson(const nlohmann::json& data);
	};

    class ClusterNode
    {
    public:
    	double parentMaxLODError = -1;
    	double lodError = -1;
    	glm::vec3 boundingSphereCenter;
    	float boundingSphereRadius;

	    nlohmann::json toJson();
    	void fromJson(const nlohmann::json& data);
    	
    };
    

    
}
