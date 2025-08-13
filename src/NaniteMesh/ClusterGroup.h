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
    	std::vector<idx_t> localTriangleClusterIndices;
    	std::vector<NaniteTriMesh::HalfedgeHandle> clusterGroupHalfedges;
    	const int targetClusterSize = CLUSTER_SIZE;
    	idx_t localClusterNum;
    	Graph localTriangleGraph;
	
    	std::vector<uint32_t> triangleIndicesLocalGlobalMap; 
    	std::unordered_map<uint32_t, uint32_t> triangleIndicesGlobalLocalMap;

    	float qemError;

    	void buildTriangleIndicesLocalGlobalMapping();
    	void buildLocalTriangleGraph();
    	void generateLocalClusters();
    };    
    
}

