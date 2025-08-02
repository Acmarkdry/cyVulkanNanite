#pragma once
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <glm/detail/type_vec.hpp>

#include "Const.h"

namespace Nanite
{
	
    class ClusterGroup
    {
    public:
    	NaniteTriMesh * mesh;
    	OpenMesh::HPropHandleT<int32_t> clusterGroupIndexPropHandle;
		
		// n-1级别的cluster indices
    	std::vector<uint32_t> clusterIndices;
		// n-1数据
    	uint32_t localFaceNum;
		// mesh lod n级别的cluster group数据
    	std::unordered_set<NaniteTriMesh::FaceHandle> clusterGroupFaces;
    	// mesh lod n级别
    	std::vector<NaniteTriMesh::HalfedgeHandle> clusterGroupHalfedges;
	
    	// Local triangle graph partition related
    	std::vector<idx_t> localTriangleClusterIndices;
    	idx_t localClusterNum;
    	Graph localTriangleGraph;
    	
    	std::vector<uint32_t> triangleIndicesLocalGlobalMap; 
    	std::unordered_map<uint32_t, uint32_t> triangleIndicesGlobalLocalMap;

    	float qemError;
    	glm::vec3 pMin = glm::vec3(FLT_MAX);
    	glm::vec3 pMax = glm::vec3(-FLT_MAX);

    	void buildTriangleIndicesLocalGlobalMapping();
    	void buildLocalTriangleGraph();
    	void generateLocalClusters();
    	void mergeAABB(const glm::vec3& pMinOther, const glm::vec3& pMaxOther);
    };    
    
}

