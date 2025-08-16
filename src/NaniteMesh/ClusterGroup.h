#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Const.h"

namespace Nanite
{
	class ClusterGroup
	{
	public:
		// 网格引用
		NaniteTriMesh* mesh = nullptr;
		OpenMesh::HPropHandleT<int32_t> clusterGroupIndexPropHandle;

		// n-1级别的cluster indices
		std::vector<uint32_t> clusterIndices;
		uint32_t localFaceNum = 0;

		// mesh lod n级别的cluster group数据
		std::unordered_set<NaniteTriMesh::FaceHandle> clusterGroupFaces;
		std::vector<idx_t> localTriangleClusterIndices;
		std::vector<NaniteTriMesh::HalfedgeHandle> clusterGroupHalfedges;

		static constexpr int TARGET_CLUSTER_SIZE = CLUSTER_SIZE;
		static constexpr idx_t METIS_RANDOM_SEED = 42;

		idx_t localClusterNum = 0;
		Graph localTriangleGraph;

		// 索引映射
		std::vector<uint32_t> triangleIndicesLocalGlobalMap;
		std::unordered_map<uint32_t, uint32_t> triangleIndicesGlobalLocalMap;

		float qemError = 0.0f;

		void buildTriangleIndicesLocalGlobalMapping();
		void buildLocalTriangleGraph();
		void generateLocalClusters();

	private:
		[[nodiscard]] idx_t calculateEmbeddedSize() const noexcept;
	};
}
