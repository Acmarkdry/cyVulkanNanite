#include "ClusterGroup.h"
#include "TaskGraph.h"
#include "utils.h"

#include <algorithm>

namespace Nanite
{
	void ClusterGroup::buildTriangleIndicesLocalGlobalMapping()
	{
		const auto faceCount = clusterGroupFaces.size();

		// Ԥ�������������������·���
		triangleIndicesLocalGlobalMap.clear();
		triangleIndicesLocalGlobalMap.reserve(faceCount);
		triangleIndicesGlobalLocalMap.clear();
		triangleIndicesGlobalLocalMap.reserve(faceCount);

		uint32_t localTriangleIndex = 0;
		for (const auto& face : clusterGroupFaces)
		{
			const auto globalIdx = static_cast<uint32_t>(face.idx());
			triangleIndicesLocalGlobalMap.emplace_back(globalIdx);
			triangleIndicesGlobalLocalMap.emplace(globalIdx, localTriangleIndex++);
		}
	}

	idx_t ClusterGroup::calculateEmbeddedSize() const noexcept
	{
		const auto faceCount = static_cast<idx_t>(clusterGroupFaces.size());
		return ((faceCount + TARGET_CLUSTER_SIZE - 1) / TARGET_CLUSTER_SIZE) * TARGET_CLUSTER_SIZE;
	}

	void ClusterGroup::buildLocalTriangleGraph()
	{
		const auto embeddedSize = calculateEmbeddedSize();
		localTriangleGraph.resize(embeddedSize);

		for (const auto& heh : clusterGroupHalfedges)
		{
			const auto fh1 = mesh->face_handle(heh);
			const auto fh2 = mesh->opposite_face_handle(heh);

			if (!fh1.is_valid() || !fh2.is_valid()) continue;

			const auto oppositeHeh = mesh->opposite_halfedge_handle(heh);
			const auto groupIdx1 = mesh->property(clusterGroupIndexPropHandle, heh) - 1;
			const auto groupIdx2 = mesh->property(clusterGroupIndexPropHandle, oppositeHeh) - 1;

			// ֻ����ͬһcluster group�ڵı�
			if (groupIdx1 != groupIdx2) continue;

			const auto localIdx1 = triangleIndicesGlobalLocalMap.at(fh1.idx());
			const auto localIdx2 = triangleIndicesGlobalLocalMap.at(fh2.idx());

			// ����˫���
			localTriangleGraph.addEdge(localIdx1, localIdx2, 1);
			localTriangleGraph.addEdge(localIdx2, localIdx1, 1);
		}
	}

	void ClusterGroup::generateLocalClusters()
	{
		auto metisGraph = MetisGraph::GraphToMetisGraph(localTriangleGraph);
		const auto vertexCount = metisGraph.nvtxs;

		localTriangleClusterIndices.resize(vertexCount);

		const auto clusterSize = std::min(TARGET_CLUSTER_SIZE, vertexCount);
		localClusterNum = vertexCount / clusterSize;

		// ��һ�������⴦��
		if (localClusterNum <= 1)
		{
			std::fill(localTriangleClusterIndices.begin(), localTriangleClusterIndices.end(), 0);
			return;
		}

		// ʹ��vector���malloc���Զ��ڴ����
		idx_t ncon = 1;
		std::vector<real_t> tpwgts(ncon * localClusterNum);

		const auto weightPerCluster = static_cast<real_t>(clusterSize) / vertexCount;
		std::fill(tpwgts.begin(), tpwgts.end(), weightPerCluster);

		// METIS分区（METIS 内部有全局状态，需要加锁保证线程安全）
		idx_t options[METIS_NOPTIONS];
		METIS_SetDefaultOptions(options);
		options[METIS_OPTION_SEED] = METIS_RANDOM_SEED;

		idx_t objVal = 0;
		idx_t result;
		{
			std::lock_guard lock(GetMetisMutex());
			result = METIS_PartGraphKway(&metisGraph.nvtxs, &ncon, metisGraph.xadj.data(), metisGraph.adjncy.data(), nullptr, // vwgt
			                                        nullptr, // vsize
			                                        metisGraph.adjwgt.data(), &localClusterNum, tpwgts.data(), nullptr, // ubvec
			                                        options, &objVal, localTriangleClusterIndices.data());
		}

		NaniteAssert(result == METIS_OK, "METIS_PartGraphKway failed");
	}

	void ClusterGroup::mergeAABB(const glm::vec3& pMinOther, const glm::vec3& pMaxOther)
	{
		pMin = glm::min(pMin, pMinOther);
		pMax = glm::max(pMax, pMaxOther);
	}
}
