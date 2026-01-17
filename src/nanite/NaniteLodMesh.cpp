#include "NaniteLodMesh.h"

#include <algorithm>
#include <numeric>
#include <stack>
#include <unordered_set>

#include "Cluster.h"
#include "ClusterGroup.h"
#include "TaskGraph.h"
#include "utils.h"
#include "vksTools.h"
#include "metis.h"
#include "NaniteBVH.h"

namespace Nanite
{
	glm::vec3 NaniteLodMesh::pointToVec3(const NaniteTriMesh::Point& p) const
	{
		return glm::vec3(p[0], p[1], p[2]);
	}

	NaniteLodMesh::FaceVertices NaniteLodMesh::getFaceVertices(NaniteTriMesh::FaceHandle fh) const
	{
		auto fv_it = mesh.cfv_iter(fh);
		FaceVertices fv;
		fv.v0 = *fv_it++;
		fv.v1 = *fv_it++;
		fv.v2 = *fv_it;
		return fv;
	}

	void NaniteLodMesh::sortTrianglesByCluster()
	{
		const auto faceCount = mesh.n_faces();
		triangleIndicesSortedByClusterIdx.resize(faceCount);
		std::iota(triangleIndicesSortedByClusterIdx.begin(), 
		          triangleIndicesSortedByClusterIdx.end(), 0u);

		std::sort(triangleIndicesSortedByClusterIdx.begin(), 
		          triangleIndicesSortedByClusterIdx.end(),
		          [this](uint32_t a, uint32_t b) {
		              return triangleClusterIndex[a] < triangleClusterIndex[b];
		          });
	}

	void NaniteLodMesh::buildTriangleVertexIndices()
	{
		const auto faceCount = triangleIndicesSortedByClusterIdx.size();
		triangleVertexIndicesSortedByClusterIdx.resize(faceCount * 3);

		for (size_t i = 0; i < faceCount; ++i)
		{
			const auto triangleIndex = triangleIndicesSortedByClusterIdx[i];
			const auto face = mesh.face_handle(triangleIndex);
			const auto fv = getFaceVertices(face);
			
			const size_t baseIdx = i * 3;
			triangleVertexIndicesSortedByClusterIdx[baseIdx] = fv.v0.idx();
			triangleVertexIndicesSortedByClusterIdx[baseIdx + 1] = fv.v1.idx();
			triangleVertexIndicesSortedByClusterIdx[baseIdx + 2] = fv.v2.idx();
		}
	}

	void NaniteLodMesh::initClustersFromFaces()
	{
		clusters.resize(clusterNum);
		for (auto face_it = mesh.faces_begin(); face_it != mesh.faces_end(); ++face_it)
		{
			const auto fh = *face_it;
			const auto clusterIdx = triangleClusterIndex[fh.idx()];
			clusters[clusterIdx].triangleIndices.emplace_back(fh.idx());
		}
	}

	void NaniteLodMesh::assignTriangleClusterGroup(NaniteLodMesh& lastLOD)
	{
		// ������һ��LOD��cluster group��Ϣ
		for (size_t i = 0; i < lastLOD.clusterGroups.size(); ++i)
		{
			oldClusterGroups[i].clusterIndices = lastLOD.clusterGroups[i].clusterIndices;
			oldClusterGroups[i].qemError = lastLOD.clusterGroups[i].qemError;
		}

		// �����ߵ�cluster group
		for (const auto& heh : mesh.halfedges())
		{
			if (mesh.is_boundary(heh)) continue;
			
			const auto clusterGroupIdx = mesh.property(clusterGroupIndexPropHandle, heh) - 1;
			oldClusterGroups[clusterGroupIdx].clusterGroupHalfedges.emplace_back(heh);
			oldClusterGroups[clusterGroupIdx].clusterGroupFaces.insert(mesh.face_handle(heh));
		}

		triangleClusterIndex.resize(mesh.n_faces(), -1);
		uint32_t clusterIndexOffset = 0;
		std::vector<std::unordered_set<uint32_t>> newClusterIndicesSet(oldClusterGroups.size());

		// Phase 1: 并行处理每个 ClusterGroup 的局部图构建和分区
		// 每个 ClusterGroup 操作自己的局部数据，mesh 属性只读，无竞争
		for (size_t i = 0; i < oldClusterGroups.size(); ++i)
		{
			auto& oldClusterGroup = oldClusterGroups[i];
			oldClusterGroup.clusterGroupIndexPropHandle = clusterGroupIndexPropHandle;
			oldClusterGroup.mesh = &mesh;
		}

		ParallelFor(static_cast<int32_t>(oldClusterGroups.size()), [this](int32_t i)
		{
			auto& oldClusterGroup = oldClusterGroups[i];
			oldClusterGroup.buildTriangleIndicesLocalGlobalMapping();
			oldClusterGroup.buildLocalTriangleGraph();
			oldClusterGroup.generateLocalClusters();
		});

		// Phase 2: 串行合并结果（需要累加 clusterIndexOffset）
		for (size_t i = 0; i < oldClusterGroups.size(); ++i)
		{
			auto& oldClusterGroup = oldClusterGroups[i];

			for (const auto& fh : oldClusterGroup.clusterGroupFaces)
			{
				const auto localTriangleIdx = oldClusterGroup.triangleIndicesGlobalLocalMap[fh.idx()];
				NaniteAssert(triangleClusterIndex[fh.idx()] < 0, "Repeat clustering");
				
				const uint32_t clusterIdx = clusterIndexOffset + oldClusterGroup.localTriangleClusterIndices[localTriangleIdx];
				triangleClusterIndex[fh.idx()] = clusterIdx;
				newClusterIndicesSet[i].emplace(clusterIdx);
			}

			std::vector<uint32_t> newClusterIndices(newClusterIndicesSet[i].begin(), newClusterIndicesSet[i].end());
			for (const auto idx : oldClusterGroup.clusterIndices)
			{
				lastLOD.clusters[idx].parentClusterIndices = newClusterIndices;
			}
			clusterIndexOffset += oldClusterGroup.localClusterNum;
		}

		// ��֤�����������ѷ���
		for (size_t i = 0; i < triangleClusterIndex.size(); ++i)
		{
			NaniteAssert(triangleClusterIndex[i] >= 0, "triangleClusterIndex[i] < 0");
		}

		clusterNum = *std::max_element(triangleClusterIndex.begin(), triangleClusterIndex.end()) + 1;
		
		sortTrianglesByCluster();
		buildTriangleVertexIndices();
		initClustersFromFaces();

		// �������ӹ�ϵ
		for (size_t i = 0; i < lastLOD.clusters.size(); ++i)
		{
			for (const int idx : lastLOD.clusters[i].parentClusterIndices)
			{
				clusters[idx].childClusterIndices.emplace_back(i);
			}
		}

		// ����QEM���
		for (size_t i = 0; i < oldClusterGroups.size(); ++i)
		{
			for (const auto& newClusterIndex : newClusterIndicesSet[i])
			{
				clusters[newClusterIndex].qemError = oldClusterGroups[i].qemError;
			}
		}

		// �����Χ��ͱ����
		for (auto& cluster : clusters)
		{
			calcBoundingSphereFromChildren(cluster, lastLOD);
			calcSurfaceArea(cluster);
			for (const int idx : cluster.childClusterIndices)
			{
				cluster.boundingSphereRadius = glm::max(cluster.boundingSphereRadius, 
					lastLOD.clusters[idx].boundingSphereRadius * 2.0f);
			}
		}
		
		for (auto & childClusters: lastLOD.clusters)
		{
			auto firstParent = childClusters.parentClusterIndices[0];
			childClusters.parentBoundingSphereCenter = clusters[firstParent].boundingSphereCenter;
			childClusters.parentBoundingSphereRadius= clusters[firstParent].boundingSphereRadius;
		}

		// ����LOD���
		for (auto& cluster : clusters)
		{
			cluster.lodLevel = lodLevel;
			double maxChildNormalizedError = 0.0;
			
			for (const int idx : cluster.childClusterIndices)
			{
				const auto& childCluster = lastLOD.clusters[idx];
				cluster.childLODErrorMax = std::max(cluster.childLODErrorMax, childCluster.lodError);
				maxChildNormalizedError = std::max(maxChildNormalizedError, childCluster.normalizedlodError);
			}
			
			cluster.childLODErrorMax = std::max(cluster.childLODErrorMax, 0.0);
			NaniteAssert(cluster.childLODErrorMax >= 0, "cluster.childMaxLODError < 0");
			NaniteAssert(cluster.qemError >= 0, "cluster.qemError < 0");
			
			const auto parentCount = lastLOD.clusters[cluster.childClusterIndices[0]].parentClusterIndices.size();
        cluster.lodError = cluster.qemError / (lastLOD.clusters[cluster.childClusterIndices[0]].parentClusterIndices.size()+1) + cluster.childLODErrorMax;
			cluster.normalizedlodError = std::max(maxChildNormalizedError + 1e-9, 
				cluster.lodError / (cluster.boundingSphereRadius * cluster.boundingSphereRadius));

			for (const int idx : cluster.childClusterIndices)
			{
				auto& childCluster = lastLOD.clusters[idx];
				NaniteAssert(childCluster.parentNormalizedError < 0 || 
					std::abs(childCluster.parentNormalizedError - cluster.normalizedlodError) < FLT_EPSILON,
					"Parents have different lod error");
				NaniteAssert(cluster.surfaceArea > DBL_EPSILON, "cluster.surfaceArea <= 0");
				childCluster.parentNormalizedError = cluster.normalizedlodError;
				childCluster.parentSurfaceArea = cluster.surfaceArea;
			}
		}
	}

	void NaniteLodMesh::buildTriangleGraph()
	{
		const auto faceCount = mesh.n_faces();
		const int embeddingSize = targetClusterSize * (1 + (faceCount + 1) / targetClusterSize) - faceCount;
		triangleGraph.resize(faceCount + embeddingSize);
		isLastLODEdgeVertices.resize(faceCount * 3, false);

		// ��Ǳ߽綥��
		for (const auto& edge : mesh.edges())
		{
			const auto heh = mesh.halfedge_handle(edge, 0);
			const auto groupIdx1 = mesh.property(clusterGroupIndexPropHandle, heh) - 1;
			const auto groupIdx2 = mesh.property(clusterGroupIndexPropHandle, mesh.opposite_halfedge_handle(heh)) - 1;

			if (groupIdx1 != groupIdx2)
			{
				const auto fh = mesh.face_handle(heh);
				const auto vh1 = mesh.to_vertex_handle(heh);
				const auto vh2 = mesh.to_vertex_handle(mesh.opposite_halfedge_handle(heh));

				for (auto fv_it = mesh.cfv_iter(fh); fv_it.is_valid(); ++fv_it)
				{
					if (*fv_it == vh1 || *fv_it == vh2)
						isLastLODEdgeVertices[fv_it->idx()] = true;
				}
			}
		}

		// ������żͼ
		for (const auto& edge : mesh.edges())
		{
			const auto heh = mesh.halfedge_handle(edge, 0);
			const auto fh = mesh.face_handle(heh);
			const auto fh2 = mesh.opposite_face_handle(heh);
			
			if (!fh.is_valid() || !fh2.is_valid()) continue;
			
			triangleGraph.addEdge(fh.idx(), fh2.idx(), 1);
			triangleGraph.addEdge(fh2.idx(), fh.idx(), 1);
		}
	}

	void NaniteLodMesh::generateCluster()
	{
		auto triangleMetisGraph = MetisGraph::GraphToMetisGraph(triangleGraph);
		const auto vertexCount = triangleMetisGraph.nvtxs;

		triangleClusterIndex.resize(vertexCount);

		const int clusterSize = std::min(targetClusterSize, vertexCount);
		clusterNum = vertexCount / clusterSize;

		idx_t ncon = 1;
		std::vector<real_t> tpwgts(ncon * clusterNum, 1.0f / clusterNum);

		idx_t objVal;
		idx_t options[METIS_NOPTIONS];
		METIS_SetDefaultOptions(options);
		options[METIS_OPTION_SEED] = METIS_RANDOM_SEED;

		const auto res = METIS_PartGraphKway(
			&triangleMetisGraph.nvtxs, &ncon,
			triangleMetisGraph.xadj.data(), triangleMetisGraph.adjncy.data(),
			nullptr, nullptr, triangleMetisGraph.adjwgt.data(),
			&clusterNum, tpwgts.data(), nullptr, options,
			&objVal, triangleClusterIndex.data());
		
		NaniteAssert(res == METIS_OK, "METIS_PartGraphKway failed");

		sortTrianglesByCluster();
		buildTriangleVertexIndices();

		clusters.resize(clusterNum);
		for (auto face_it = mesh.faces_begin(); face_it != mesh.faces_end(); ++face_it)
		{
			const auto fh = *face_it;
			const auto clusterIdx = triangleClusterIndex[fh.idx()];
			clusters[clusterIdx].triangleIndices.emplace_back(fh.idx());
			clusters[clusterIdx].lodLevel = lodLevel;
			clusters[clusterIdx].lodError = -1;
		}

		// 并行计算每个 cluster 的 BoundingSphere 和 SurfaceArea
		// 每个 cluster 只读 mesh 的顶点数据，写自己的成员，无竞争
		ParallelFor(static_cast<int32_t>(clusters.size()), [this](int32_t i)
		{
			getBoundingSphere(clusters[i]);
			calcSurfaceArea(clusters[i]);
		});
	}

	void NaniteLodMesh::buildClusterGraph()
	{
		const int embeddedSize = (clusterNum + targetClusterGroupSize - 1) / targetClusterGroupSize * targetClusterGroupSize;
		clusterGraph.resize(embeddedSize);

		for (const auto& edge : mesh.edges())
		{
			const auto heh = mesh.halfedge_handle(edge, 0);
			const auto fh = mesh.face_handle(heh);
			const auto fh2 = mesh.opposite_face_handle(heh);
			
			if (!fh.is_valid() || !fh2.is_valid()) continue;

			const auto clusterIdx1 = triangleClusterIndex[fh.idx()];
			const auto clusterIdx2 = triangleClusterIndex[fh2.idx()];

			if (clusterIdx1 != clusterIdx2)
			{
				clusterGraph.addEdgeCost(clusterIdx1, clusterIdx2, 1);
				clusterGraph.addEdgeCost(clusterIdx2, clusterIdx1, 1);
			}
		}
	}

	void NaniteLodMesh::colorClusterGraph()
	{
		std::vector<int> clusterSortedByConnectivity(clusterNum);
		std::iota(clusterSortedByConnectivity.begin(), clusterSortedByConnectivity.end(), 0);

		std::sort(clusterSortedByConnectivity.begin(), clusterSortedByConnectivity.end(),
			[this](int a, int b) {
				return clusterGraph.adjMap[a].size() > clusterGraph.adjMap[b].size();
			});

		for (const int clusterIndex : clusterSortedByConnectivity)
		{
			std::unordered_set<int> neighbor_colors;
			for (const auto& [neighbor, cost] : clusterGraph.adjMap[clusterIndex])
			{
				if (auto it = clusterColorAssignment.find(neighbor); it != clusterColorAssignment.end())
					neighbor_colors.insert(it->second);
			}

			int color = 0;
			while (neighbor_colors.contains(color))
				++color;

			clusterColorAssignment[clusterIndex] = color;
		}
	}

	void NaniteLodMesh::simplifyMesh(NaniteTriMesh& mymesh)
	{
		OpenMesh::Decimater::DecimaterT<NaniteTriMesh> decimater(mymesh);
		OpenMesh::Decimater::MyModQuadricT<NaniteTriMesh>::Handle hModQuadric;
		decimater.add(hModQuadric);
		decimater.module(hModQuadric).set_max_err(FLT_MAX, false);
		decimater.initialize();

		std::cout << "NUM FACES BEFORE: " << mymesh.n_faces() << std::endl;

		for (size_t i = 0; i < clusterGroups.size(); ++i)
		{
			const auto currTargetFaceNum = static_cast<size_t>(
				mymesh.n_faces() - clusterGroups[i].localFaceNum * (1.0 - SIMPLIFY_PERCENTAGE));

			// ��Ƕ���
			for (const auto& heh : mymesh.halfedges())
			{
				const auto clusterGroupIdx = mymesh.property(clusterGroupIndexPropHandle, heh) - 1;
				if (clusterGroupIdx != static_cast<int>(i)) continue;

				const auto vh1 = mymesh.to_vertex_handle(heh);
				const auto vh2 = mymesh.from_vertex_handle(heh);
				mymesh.status(vh1).set_selected(true);
				mymesh.status(vh2).set_selected(true);

				const auto oppositeHeh = mymesh.opposite_halfedge_handle(heh);
				if (mymesh.is_boundary(oppositeHeh) ||
					(mymesh.property(clusterGroupIndexPropHandle, oppositeHeh) - 1) != static_cast<int>(i))
				{
					mymesh.status(vh1).set_locked(true);
					mymesh.status(vh2).set_locked(true);
				}
			}

			const auto n_collapses = decimater.decimate_to_faces(0, currTargetFaceNum, true);
			clusterGroups[i].qemError = decimater.module(hModQuadric).total_err();
			decimater.module(hModQuadric).clear_total_err();

			mymesh.garbage_collection();
			for (const auto& vh : mymesh.vertices())
				mymesh.status(vh).set_selected(false);
		}

		for (const auto& vh : mymesh.vertices())
		{
			mymesh.status(vh).set_selected(false);
			mymesh.status(vh).set_locked(false);
		}

		mymesh.garbage_collection();
		std::cout << "NUM FACES AFTER: " << mymesh.n_faces() << std::endl;
	}

	void NaniteLodMesh::calcBoundingSphereFromChildren(Cluster& cluster, NaniteLodMesh& lastLOD)
	{
		glm::vec3 center(0.0f);
		float max_radius = 0.0f;

		for (const auto idx : cluster.childClusterIndices)
		{
			const auto& childCluster = lastLOD.clusters[idx];
			center += childCluster.boundingSphereCenter;
			max_radius = std::max(max_radius, childCluster.boundingSphereRadius);
		}

		center /= static_cast<float>(cluster.childClusterIndices.size());
		cluster.boundingSphereCenter = center;
		cluster.boundingSphereRadius = max_radius;
	}

	void NaniteLodMesh::getBoundingSphere(Cluster& cluster)
	{
		if (cluster.triangleIndices.empty())
		{
			cluster.boundingSphereCenter = glm::vec3(0.0f);
			cluster.boundingSphereRadius = 0.0f;
			return;
		}

		const auto& triangleIndices = cluster.triangleIndices;
		auto px = *mesh.fv_begin(mesh.face_handle(triangleIndices[0]));
		NaniteTriMesh::VertexHandle py, pz;
		float dist2_max = -1.0f;

		// ����Զ��py
		for (const auto triangleIndex : triangleIndices)
		{
			const auto fv = getFaceVertices(mesh.face_handle(triangleIndex));
			for (const auto& vh : {fv.v0, fv.v1, fv.v2})
			{
				const float dist2 = (mesh.point(vh) - mesh.point(px)).sqrnorm();
				if (dist2 > dist2_max)
				{
					dist2_max = dist2;
					py = vh;
				}
			}
		}

		// ����py��Զ�ĵ�pz
		dist2_max = -1.0f;
		for (const auto triangleIndex : triangleIndices)
		{
			const auto fv = getFaceVertices(mesh.face_handle(triangleIndex));
			for (const auto& vh : {fv.v0, fv.v1, fv.v2})
			{
				const float dist2 = (mesh.point(vh) - mesh.point(py)).sqrnorm();
				if (dist2 > dist2_max)
				{
					dist2_max = dist2;
					pz = vh;
				}
			}
		}

		auto c = (mesh.point(py) + mesh.point(pz)) / 2.0f;
		auto r = std::sqrt(dist2_max) / 2.0f;

		// ��չ��Χ���԰������е�
		for (const auto triangleIndex : triangleIndices)
		{
			const auto fv = getFaceVertices(mesh.face_handle(triangleIndex));
			for (const auto& vh : {fv.v0, fv.v1, fv.v2})
			{
				const float dist2 = (mesh.point(vh) - c).sqrnorm();
				if (dist2 > r * r)
					r = std::sqrt(dist2);
			}
		}

		cluster.boundingSphereCenter = pointToVec3(c);
		cluster.boundingSphereRadius = r;
		NaniteAssert(cluster.boundingSphereRadius > 0, "cluster.boundingSphereRadius <= 0");
	}

	void NaniteLodMesh::calcSurfaceArea(Cluster& cluster)
	{
		cluster.surfaceArea = std::accumulate(
			cluster.triangleIndices.begin(), cluster.triangleIndices.end(), 0.0,
			[this](double sum, uint32_t idx) {
				return sum + mesh.calc_face_area(mesh.face_handle(idx));
			});
	}

	nlohmann::json NaniteLodMesh::toJson()
	{
		nlohmann::json result = {
			{"clusterNum", clusterNum},
			{"triangleClusterIndex", triangleClusterIndex},
			{"clusterColorAssignment", clusterColorAssignment},
			{"clusterGroupIndex", clusterGroupIndex},
			{"triangleIndicesSortedByClusterIdx", triangleIndicesSortedByClusterIdx},
			{"triangleVertexIndicesSortedByClusterIdx", triangleVertexIndicesSortedByClusterIdx},
			{"clusters", nlohmann::json::array()}
		};

		for (auto& cluster : clusters)
			result["clusters"].push_back(cluster.toJson());

		return result;
	}

	void NaniteLodMesh::fromJson(const nlohmann::json& j)
	{
		clusterNum = j["clusterNum"].get<int>();
		clusterColorAssignment = j["clusterColorAssignment"].get<std::unordered_map<int, int>>();
		triangleClusterIndex = j["triangleClusterIndex"].get<std::vector<idx_t>>();
		clusterGroupIndex = j["clusterGroupIndex"].get<std::vector<idx_t>>();
		triangleIndicesSortedByClusterIdx = j["triangleIndicesSortedByClusterIdx"].get<std::vector<uint32_t>>();
		triangleVertexIndicesSortedByClusterIdx = j["triangleVertexIndicesSortedByClusterIdx"].get<std::vector<uint32_t>>();

		clusters.resize(clusterNum);
		for (size_t i = 0; i < clusters.size(); ++i)
			clusters[i].fromJson(j["clusters"][i]);
	}

	void NaniteLodMesh::generateClusterGroup()
	{
		auto clusterMetisGraph = MetisGraph::GraphToMetisGraph(clusterGraph);
		clusterGroupIndex.resize(clusterMetisGraph.nvtxs);

		idx_t ncon = 1;
		clusterGroupNum = clusterMetisGraph.nvtxs / targetClusterGroupSize;
		clusterGroups.resize(clusterGroupNum);

		if (clusterGroupNum == 1)
		{
			std::fill(clusterGroupIndex.begin(), clusterGroupIndex.end(), 0);
			for (size_t i = 0; i < static_cast<size_t>(clusterMetisGraph.nvtxs); ++i)
				clusterGroups[0].clusterIndices.emplace_back(i);

			for (auto& heh : mesh.halfedges())
			{
				if (!mesh.is_boundary(heh))
					mesh.property(clusterGroupIndexPropHandle, heh) = 1;
			}
			return;
		}

		std::vector<real_t> tpwgts(ncon * clusterGroupNum, 
			static_cast<float>(targetClusterGroupSize) / clusterMetisGraph.nvtxs);

		idx_t objVal;
		idx_t options[METIS_NOPTIONS];
		METIS_SetDefaultOptions(options);
		options[METIS_OPTION_SEED] = METIS_RANDOM_SEED;

		const auto res = METIS_PartGraphKway(
			&clusterMetisGraph.nvtxs, &ncon,
			clusterMetisGraph.xadj.data(), clusterMetisGraph.adjncy.data(),
			nullptr, nullptr, clusterMetisGraph.adjwgt.data(),
			&clusterGroupNum, tpwgts.data(), nullptr, options,
			&objVal, clusterGroupIndex.data());

		NaniteAssert(res == METIS_OK, "METIS_PartGraphKway failed");

		for (size_t clusterIdx = 0; clusterIdx < static_cast<size_t>(clusterNum); ++clusterIdx)
		{
			const auto clusterGroupIdx = clusterGroupIndex[clusterIdx];
			clusters[clusterIdx].clusterGroupIndex = clusterGroupIdx;
			clusterGroups[clusterGroupIdx].clusterIndices.emplace_back(clusterIdx);
		}

		isEdgeVertices.resize(mesh.n_faces() * 3, false);
		std::vector<std::unordered_set<NaniteTriMesh::FaceHandle>> clusterGroupFaceHandles(clusterGroupNum);

		for (const auto& edge : mesh.edges())
		{
			const auto heh = mesh.halfedge_handle(edge, 0);
			const auto oppositeHeh = mesh.opposite_halfedge_handle(heh);
			const auto fh = mesh.face_handle(heh);
			const auto fh2 = mesh.opposite_face_handle(heh);

			if (mesh.is_boundary(oppositeHeh) && fh.is_valid())
			{
				const auto groupIdx = clusterGroupIndex[triangleClusterIndex[fh.idx()]];
				mesh.property(clusterGroupIndexPropHandle, heh) = groupIdx + 1;
				clusterGroupFaceHandles[groupIdx].insert(fh);
				continue;
			}

			if (mesh.is_boundary(heh) && fh2.is_valid())
			{
				const auto groupIdx = clusterGroupIndex[triangleClusterIndex[fh2.idx()]];
				mesh.property(clusterGroupIndexPropHandle, heh) = groupIdx + 1;
				clusterGroupFaceHandles[groupIdx].insert(fh2);
				continue;
			}

			if (!fh.is_valid() || !fh2.is_valid()) continue;

			const auto groupIdx1 = clusterGroupIndex[triangleClusterIndex[fh.idx()]];
			const auto groupIdx2 = clusterGroupIndex[triangleClusterIndex[fh2.idx()]];

			clusterGroupFaceHandles[groupIdx1].insert(fh);
			clusterGroupFaceHandles[groupIdx2].insert(fh2);
			mesh.property(clusterGroupIndexPropHandle, heh) = groupIdx1 + 1;
			mesh.property(clusterGroupIndexPropHandle, oppositeHeh) = groupIdx2 + 1;
		}

		for (size_t i = 0; i < static_cast<size_t>(clusterGroupNum); ++i)
			clusterGroups[i].localFaceNum = clusterGroupFaceHandles[i].size();
	}

	void NaniteLodMesh::initVertexBuffer()
	{
		vertexBuffer.reserve(mesh.n_faces() * 3);

		for (auto f_it = mesh.faces_begin(); f_it != mesh.faces_end(); ++f_it)
		{
			const auto face = *f_it;
			const int clusterId = triangleClusterIndex[face.idx()];
			const auto color = nodeColors[clusterColorAssignment[clusterId] % nodeColors.size()];

			for (auto fv_it = mesh.cfv_iter(face); fv_it.is_valid(); ++fv_it)
			{
				vkglTF::Vertex v;
				v.pos = pointToVec3(mesh.point(*fv_it));
				v.normal = pointToVec3(mesh.normal(*fv_it));
				v.uv = glm::vec2(mesh.texcoord2D(*fv_it)[0], mesh.texcoord2D(*fv_it)[1]);
				v.joint0 = glm::vec4(color, static_cast<float>(clusterId));
				vertexBuffer.emplace_back(v);
			}
		}
	}

	void NaniteLodMesh::initUniqueVertexBuffer()
	{
		uniqueVertexBuffer.reserve(mesh.n_vertices());

		for (const auto& vertex : mesh.vertices())
		{
			vkglTF::Vertex v;
			v.pos = pointToVec3(mesh.point(vertex));
			v.normal = pointToVec3(mesh.normal(vertex));
			v.uv = glm::vec2(mesh.texcoord2D(vertex)[0], mesh.texcoord2D(vertex)[1]);
			v.joint0 = glm::vec4(static_cast<float>(lodLevel));
			v.weight0 = glm::vec4(0.0f);
			uniqueVertexBuffer.emplace_back(v);
		}
	}

	void NaniteLodMesh::createVertexBuffer(VulkanExampleBase& variableLink)
	{
		const size_t vertexBufferSize = vertexBuffer.size() * sizeof(vkglTF::Vertex);
		vertices.count = static_cast<uint32_t>(vertexBuffer.size());
		vks::vksTools::createStagingBuffer(variableLink, 0, vertexBufferSize, vertexBuffer.data(), 
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices);
	}

	void NaniteLodMesh::createBVH()
	{
		buildBVH();
		updateBVHError();
	}

	void NaniteLodMesh::getClusterGroupAABB(ClusterGroup& clusterGroup)
	{
		glm::vec3 min = glm::vec3(FLT_MAX);
		glm::vec3 max = glm::vec3(-FLT_MAX);
		for (NaniteTriMesh::FaceIter face_it = mesh.faces_begin(); face_it != mesh.faces_end(); ++face_it) {
			NaniteTriMesh::FaceHandle fh = *face_it;
			auto clusterIdx = triangleClusterIndex[fh.idx()];
			auto clusterGroupIdx = clusterGroupIndex[clusterIdx];

			glm::vec3 pMinWorld, pMaxWorld;
			glm::vec3 p0, p1, p2;
			NaniteTriMesh::FaceVertexIter fv_it = mesh.fv_iter(fh);

			// Get the positions of the three vertices
			auto point0 = mesh.point(*fv_it);
			++fv_it;
			auto point1 = mesh.point(*fv_it);
			++fv_it;
			auto point2 = mesh.point(*fv_it);

			p0[0] = point0[0];
			p0[1] = point0[1];
			p0[2] = point0[2];

			p1[0] = point1[0];
			p1[1] = point1[1];
			p1[2] = point1[2];

			p2[0] = point2[0];
			p2[1] = point2[1];
			p2[2] = point2[2];

			p0 = glm::vec3(glm::vec4(p0, 1.0f));
			p1 = glm::vec3(glm::vec4(p1, 1.0f));
			p2 = glm::vec3(glm::vec4(p2, 1.0f));

			getTriangleAABB(p0, p1, p2, pMinWorld, pMaxWorld);

			clusterGroups[clusterGroupIdx].mergeAABB(pMinWorld, pMaxWorld);
		}
	}

	void NaniteLodMesh::buildBVH()
{
    // ������򻯺󹹽�BVH����Ϊ��ʱ��ҪQEM��
    // LOD 0������ cluster �� QEM ���Ϊ -1�����̣����� cluster -> ���� cluster group -> ���� BVH
    // LOD N(N>0)������ cluster �� QEM ��� > 0�����̣��� -> �ھ� cluster group �����¾��� -> ���·��� -> ���� BVH

    std::vector<uint32_t> clusterGroupIndex;
    for (int i = 0; i < clusterGroups.size(); ++i)
    {
        clusterGroupIndex.push_back(i);
        auto& clusterGroup = clusterGroups[i];
        getClusterGroupAABB(clusterGroup);
    }

    std::stack<std::shared_ptr<NaniteBVHNode>> nodeStack;
    rootBVHNode = std::make_shared<NaniteBVHNode>();
    rootBVHNode->start = 0;
    rootBVHNode->end = clusterGroups.size();
    rootBVHNode->nodeStatus = NaniteBVHNodeStatus::NODE;
    rootBVHNode->lodLevel = lodLevel;
    nodeStack.push(rootBVHNode);

    std::set<int> clusterIndexSet;
    for (auto & clusterGroup: clusterGroups)
    {
        for (auto & clusterIndex: clusterGroup.clusterIndices)
        {
            NaniteAssert(clusterIndexSet.find(clusterIndex) == clusterIndexSet.end(), "Repeated cluster index in different cluster group!");
            clusterIndexSet.insert(clusterIndex);
        }
    }
    while (!nodeStack.empty()) 
    {
        auto & currNode = nodeStack.top();
        currNode->lodLevel = lodLevel;
        for (size_t i = 0; i < CLUSTER_GROUP_MAX_SIZE; i++)
        {
            currNode->clusterIndices[i] = -1;
        }
        std::string indent(currNode->depth, '\t');
        nodeStack.pop();
        if (currNode->nodeStatus == NaniteBVHNodeStatus::LEAF) { // Ҷ�ڵ㣬�洢 cluster group ��С�� cluster ����
            auto& clusterGroup = clusterGroups[currNode->start];
            // ��ʼ�� clusterIndices
            NaniteAssert(clusterGroup.clusterIndices.size() <= CLUSTER_GROUP_MAX_SIZE, "too many clusterIndices");
            for (size_t i = 0; i < clusterGroup.clusterIndices.size(); i++)
            {
                currNode->clusterIndices[i] = clusterGroup.clusterIndices[i];
            }
            currNode->pMin = clusterGroup.pMin;
            currNode->pMax = clusterGroup.pMax;

            for (auto clusterIndex: currNode->clusterIndices)
            {
                NaniteAssert(clusterIndex < int(clusters.size()), "clusterIndex overflow");
                if (clusterIndex >= 0) {
                    NaniteAssert(clusterIndexSet.find(clusterIndex) != clusterIndexSet.end(), "clusterIndex not found in clusterIndexSet, means it's repeated!");
                    clusterIndexSet.erase(clusterIndex);
                    currNode->normalizedlodError    = std::max(currNode->normalizedlodError, clusters[clusterIndex].normalizedlodError);
                    currNode->parentNormalizedError = std::max(currNode->parentNormalizedError, clusters[clusterIndex].parentNormalizedError);
                }
            }
        }
        else { // ��Ҷ�ڵ�
            // �ϲ�AABB
			glm::vec3 pMin = glm::vec3(FLT_MAX);
			glm::vec3 pMax = glm::vec3(-FLT_MAX);
            for (int i = currNode->start; i < currNode->end; ++i)
            {
				auto& clusterGroup = clusterGroups[i];
				pMin = glm::min(pMin, clusterGroup.pMin);
				pMax = glm::max(pMax, clusterGroup.pMax);
			}
			currNode->pMin = pMin;
			currNode->pMax = pMax;
            if (currNode->end - currNode->start < 4) { // ��Ҷ�ڵ��һ����ֹͣ�ָ�
                currNode->nodeStatus = NaniteBVHNodeStatus::NODE;
                for (int i = currNode->start; i < currNode->end; ++i)
                {
                    std::shared_ptr<NaniteBVHNode> leafNode(new NaniteBVHNode());
                    leafNode->nodeStatus = NaniteBVHNodeStatus::LEAF;
                    leafNode->start = i;
                    leafNode->end = i + 1;
                    leafNode->depth = currNode->depth + 1;
                    currNode->children.push_back(leafNode);
                }
                for (auto & child: currNode->children)
                {
                    nodeStack.push(child);
                }
            }
            else { // ��ʼ�ָ�
			    // ��ȡ���
			    glm::vec3 diff = pMax - pMin;
			    int longestAxis = 0;
			    if (diff[1] > diff[longestAxis]) longestAxis = 1;
			    if (diff[2] > diff[longestAxis]) longestAxis = 2;

			    // ���������
                std::sort(clusterGroupIndex.begin() + currNode->start, clusterGroupIndex.begin() + currNode->end, [&](uint32_t a, uint32_t b) {
				    return clusterGroups[a].pMin[longestAxis] < clusterGroups[b].pMin[longestAxis];
				    });

			    // �����ָ�
			    int mid = (currNode->start + currNode->end) / 2;

                // ��ȡ�ڶ�����
                int axis2 = (longestAxis + 1) % 3; 
                int axis3 = (longestAxis + 2) % 3;
                int secondLongestAxis = diff[axis2] > diff[axis3] ? axis2 : axis3;

                // ���ڶ���������
                std::sort(clusterGroupIndex.begin() + currNode->start, clusterGroupIndex.begin() + mid, [&](uint32_t a, uint32_t b) {
                    return clusterGroups[a].pMin[secondLongestAxis] < clusterGroups[b].pMin[secondLongestAxis];
					});
                int mid2 = (currNode->start + mid) / 2;
                
                std::sort(clusterGroupIndex.begin() + mid, clusterGroupIndex.begin() + currNode->end, [&](uint32_t a, uint32_t b) {
                    return clusterGroups[a].pMin[secondLongestAxis] < clusterGroups[b].pMin[secondLongestAxis];
                    });
                int mid3 = (mid + currNode->end) / 2;

			    std::shared_ptr<NaniteBVHNode > node11(new NaniteBVHNode());
			    node11->start = currNode->start;
			    node11->end = mid2;
                node11->depth = currNode->depth + 1;
                node11->nodeStatus = NODE;

                std::shared_ptr<NaniteBVHNode> node12(new NaniteBVHNode());
                node12->start = mid2;
                node12->end = mid;
                node12->depth = currNode->depth + 1;
                node12->nodeStatus = NODE;

                std::shared_ptr<NaniteBVHNode> node21(new NaniteBVHNode());
                node21->start = mid;
                node21->end = mid3;
                node21->depth = currNode->depth + 1;
                node21->nodeStatus = NODE;

			    std::shared_ptr<NaniteBVHNode > node22(new NaniteBVHNode());
                node22->start = mid3;
                node22->end = currNode->end;
                node22->depth = currNode->depth + 1;
                node22->nodeStatus = NODE;

                currNode->children.push_back(node11);
                currNode->children.push_back(node12);
                currNode->children.push_back(node21);
                currNode->children.push_back(node22);
                nodeStack.push(node11);
                nodeStack.push(node12);
                nodeStack.push(node21);
                nodeStack.push(node22);
            }
        }
    }

    NaniteAssert(clusterIndexSet.size() == 0, "clusterIndexSet should be empty after building BVH");

}

	void NaniteLodMesh::updateBVHError()
	{
		float currNodeError = -FLT_MAX;
		glm::vec4 currNodeParentBoundingSphere = glm::vec4(0.0f);
		updateBVHErrorCore(rootBVHNode, currNodeError, currNodeParentBoundingSphere);
	}
	
	void NaniteLodMesh::updateBVHErrorCore(std::shared_ptr<NaniteBVHNode> currNode, float & currNodeError, glm::vec4 & currNodeParentBoundingSphere)
{

    if (currNode->nodeStatus == LEAF) {
        NaniteAssert(currNode->clusterIndices.size() <= CLUSTER_GROUP_MAX_SIZE, "too many clusterIndices");
        glm::vec3 currNodeParentBoundingSphereCenter(0.0f);
        float currNodeParentBoundingSphereRadius = 0.0f;
        int validClusterNum = 0;
        double maxError = -FLT_MAX;
        std::vector<glm::vec3> childNodeParentBoundingSphereCenters;
        std::vector<float> childNodeParentBoundingSphereRadius;
        for (size_t i = 0; i < CLUSTER_GROUP_MAX_SIZE; i++)
        {
            auto clusterIndex = currNode->clusterIndices[i];
            if (clusterIndex >= 0) {
                validClusterNum++;
                currNodeParentBoundingSphereCenter += clusters[clusterIndex].parentBoundingSphereCenter;
                childNodeParentBoundingSphereCenters.push_back(clusters[clusterIndex].parentBoundingSphereCenter);
                currNodeParentBoundingSphereRadius = glm::max(currNodeParentBoundingSphereRadius, clusters[clusterIndex].parentBoundingSphereRadius);
                childNodeParentBoundingSphereRadius.push_back(clusters[clusterIndex].parentBoundingSphereRadius);
                maxError = std::max(maxError, clusters[clusterIndex].parentNormalizedError);
            }
        }
        float largestDiameter= -FLT_MAX;
        glm::vec4 sphere1(0), sphere2(0);
        for (size_t i = 0; i < childNodeParentBoundingSphereCenters.size(); i++)
        {
            for (size_t j = 0; j < childNodeParentBoundingSphereCenters.size(); j++)
            {
                auto distance = glm::distance(childNodeParentBoundingSphereCenters[i], childNodeParentBoundingSphereCenters[j]);
                auto diameter = distance + childNodeParentBoundingSphereRadius[i] + childNodeParentBoundingSphereRadius[j];
                if (diameter > largestDiameter)
                {
                    sphere1 = glm::vec4(childNodeParentBoundingSphereCenters[i], childNodeParentBoundingSphereRadius[i]);
                    sphere2 = glm::vec4(childNodeParentBoundingSphereCenters[j], childNodeParentBoundingSphereRadius[j]);
                    largestDiameter = diameter;
                }
            }
        }
        if (glm::distance(glm::vec3(sphere1), glm::vec3(sphere2)) < FLT_EPSILON) { // ���������غ�
            currNodeParentBoundingSphereCenter = glm::vec3(sphere1);
			currNodeParentBoundingSphereRadius = glm::max(sphere1.w, sphere2.w);
        }
        else {
            auto sphere1ToSphere2 = glm::normalize(glm::vec3(sphere2) - glm::vec3(sphere1));
            currNodeParentBoundingSphereCenter = 
                (glm::vec3(sphere1) + sphere1.w * sphere1ToSphere2 + glm::vec3(sphere2) + sphere2.w * -sphere1ToSphere2) * 0.5f;
            currNodeParentBoundingSphereRadius = largestDiameter * 0.5f;
        }
        currNodeError = maxError;
        
        currNodeParentBoundingSphere = glm::vec4(currNodeParentBoundingSphereCenter, currNodeParentBoundingSphereRadius);
        currNode->parentNormalizedError = currNodeError;
        currNode->parentBoundingSphere = currNodeParentBoundingSphere;
    }
    else {
        glm::vec3 currNodeParentBoundingSphereCenter(0.0f);
        float currNodeParentBoundingSphereRadius = 0.0f;
        std::vector<glm::vec3> childNodeParentBoundingSphereCenters;
        std::vector<float> childNodeParentBoundingSphereRadius;
        for (size_t i = 0; i < currNode->children.size(); i++)
        {
            auto& child = currNode->children[i];
            float childError = 0.0f;
            glm::vec4 childBoundingSphere = glm::vec4(0.0f);
            updateBVHErrorCore(child, childError, childBoundingSphere);
            currNodeError = std::max(currNodeError, childError);
            childNodeParentBoundingSphereCenters.push_back(glm::vec3(childBoundingSphere));
            childNodeParentBoundingSphereRadius.push_back(childBoundingSphere.w);
        }
        float largestDiameter = -FLT_MAX;
        glm::vec4 sphere1(0), sphere2(0);
        for (size_t i = 0; i < childNodeParentBoundingSphereCenters.size(); i++)
        {
            for (size_t j = 0; j < childNodeParentBoundingSphereCenters.size(); j++)
            {
                auto distance = glm::distance(childNodeParentBoundingSphereCenters[i], childNodeParentBoundingSphereCenters[j]);
                auto diameter = distance + childNodeParentBoundingSphereRadius[i] + childNodeParentBoundingSphereRadius[j];
                if (diameter > largestDiameter)
                {
                    sphere1 = glm::vec4(childNodeParentBoundingSphereCenters[i], childNodeParentBoundingSphereRadius[i]);
                    sphere2 = glm::vec4(childNodeParentBoundingSphereCenters[j], childNodeParentBoundingSphereRadius[j]);
                    largestDiameter = diameter;
                }
            }
        }
        if (glm::distance(glm::vec3(sphere1), glm::vec3(sphere2)) < FLT_EPSILON) { // ���������غ�
            currNodeParentBoundingSphereCenter = glm::vec3(sphere1);
            currNodeParentBoundingSphereRadius = glm::max(sphere1.w, sphere2.w);
        }
        else {
            auto sphere1ToSphere2 = glm::normalize(glm::vec3(sphere2) - glm::vec3(sphere1));
            currNodeParentBoundingSphereCenter =
                (glm::vec3(sphere1) + sphere1.w * sphere1ToSphere2 + glm::vec3(sphere2) + sphere2.w * -sphere1ToSphere2) * 0.5f;
            currNodeParentBoundingSphereRadius = largestDiameter * 0.5f;
        }
        currNodeParentBoundingSphere = glm::vec4(currNodeParentBoundingSphereCenter, currNodeParentBoundingSphereRadius);
        currNode->parentBoundingSphere = currNodeParentBoundingSphere;
        currNode->parentNormalizedError = currNodeError;
    }

    std::string indent(currNode->depth, '\t');
    std::cout << indent << "currNodeError: " << currNodeError << " boundingSphere: " << currNodeParentBoundingSphere.x << " " << currNodeParentBoundingSphere.y << " " << currNodeParentBoundingSphere.z << " " << currNodeParentBoundingSphere.w << std::endl;
}
	
}
