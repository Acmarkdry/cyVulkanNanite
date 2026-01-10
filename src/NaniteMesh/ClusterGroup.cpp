#include "ClusterGroup.h"

#include "../utils.h"

namespace Nanite
{
	void ClusterGroup::buildTriangleIndicesLocalGlobalMapping()
	{
		int localTriangleIndex = 0;
		for (const auto & clusterGroupFace: clusterGroupFaces)
		{
			triangleIndicesLocalGlobalMap.push_back(clusterGroupFace.idx());
			triangleIndicesGlobalLocalMap[clusterGroupFace.idx()] = localTriangleIndex++;
		}
	}

	void ClusterGroup::buildLocalTriangleGraph()
	{
		int embeddedSize = (clusterGroupFaces.size() + targetClusterSize - 1) / targetClusterSize * targetClusterSize;
		localTriangleGraph.resize(embeddedSize);
		for (const auto& heh : clusterGroupHalfedges)
		{
			NaniteTriMesh::FaceHandle fh = mesh->face_handle(heh);
			NaniteTriMesh::FaceHandle fh2 = mesh->opposite_face_handle(heh);
			if (fh.idx() < 0 || fh2.idx() < 0) continue;
			auto clusterGroupIdx1 = mesh->property(clusterGroupIndexPropHandle, heh) - 1;
			auto clusterGroupIdx2 = mesh->property(clusterGroupIndexPropHandle, mesh->opposite_halfedge_handle(heh)) - 1;
			if (clusterGroupIdx1 == clusterGroupIdx2) // 反边
			{
				auto localTriangleIdx1 = triangleIndicesGlobalLocalMap[fh.idx()];
				auto localTriangleIdx2 = triangleIndicesGlobalLocalMap[fh2.idx()];
				localTriangleGraph.addEdge(localTriangleIdx1, localTriangleIdx2, 1);
				localTriangleGraph.addEdge(localTriangleIdx2, localTriangleIdx1, 1);
			}
		}
	}

	void ClusterGroup::generateLocalClusters()
	{
		auto triangleMetisGraph = MetisGraph::GraphToMetisGraph(localTriangleGraph);

		localTriangleClusterIndices.resize(triangleMetisGraph.nvtxs);
		idx_t ncon = 1;

		int clusterSize = std::min(targetClusterSize, triangleMetisGraph.nvtxs);
		localClusterNum = triangleMetisGraph.nvtxs / clusterSize;
		if (localClusterNum <= 1)
		{
			for (int i = 0; i < triangleMetisGraph.nvtxs; ++i)
			{
				localTriangleClusterIndices[i] = 0;
			}
			return;
		}
		
		real_t* tpwgts = (real_t*)malloc(ncon * localClusterNum * sizeof(real_t));
		float sum = 0;
		for (idx_t i = 0; i < localClusterNum; ++i) {
			tpwgts[i] = static_cast<float>(clusterSize) / triangleMetisGraph.nvtxs; // 
			sum += tpwgts[i];
		}

		idx_t objVal;
		idx_t options[METIS_NOPTIONS];
		METIS_SetDefaultOptions(options);
		options[METIS_OPTION_SEED] = 42; 
		auto res = METIS_PartGraphKway(&triangleMetisGraph.nvtxs, &ncon, triangleMetisGraph.xadj.data(), triangleMetisGraph.adjncy.data(), nullptr, nullptr, triangleMetisGraph.adjwgt.data(), &localClusterNum, tpwgts, nullptr, options, &objVal, localTriangleClusterIndices.data());
		free(tpwgts);
		NaniteAssert(res, "METIS_PartGraphKway failed");
	}
	
}
