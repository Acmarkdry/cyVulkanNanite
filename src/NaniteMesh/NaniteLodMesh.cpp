#include "NaniteLodMesh.h"

#include <algorithm>
#include <numeric>
#include <unordered_set>

#include "Cluster.h"
#include "ClusterGroup.h"
#include "../utils.h"
#include "../vksTools.h"
#include "metis.h"

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
		// 复制上一级LOD的cluster group信息
		for (size_t i = 0; i < lastLOD.clusterGroups.size(); ++i)
		{
			oldClusterGroups[i].clusterIndices = lastLOD.clusterGroups[i].clusterIndices;
			oldClusterGroups[i].qemError = lastLOD.clusterGroups[i].qemError;
		}

		// 分配半边到cluster group
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

		for (size_t i = 0; i < oldClusterGroups.size(); ++i)
		{
			auto& oldClusterGroup = oldClusterGroups[i];
			oldClusterGroup.clusterGroupIndexPropHandle = clusterGroupIndexPropHandle;
			oldClusterGroup.mesh = &mesh;
			oldClusterGroup.buildTriangleIndicesLocalGlobalMapping();
			oldClusterGroup.buildLocalTriangleGraph();
			oldClusterGroup.generateLocalClusters();

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

		// 验证所有三角形已分配
		for (size_t i = 0; i < triangleClusterIndex.size(); ++i)
		{
			NaniteAssert(triangleClusterIndex[i] >= 0, "triangleClusterIndex[i] < 0");
		}

		clusterNum = *std::max_element(triangleClusterIndex.begin(), triangleClusterIndex.end()) + 1;
		
		sortTrianglesByCluster();
		buildTriangleVertexIndices();
		initClustersFromFaces();

		// 建立父子关系
		for (size_t i = 0; i < lastLOD.clusters.size(); ++i)
		{
			for (const int idx : lastLOD.clusters[i].parentClusterIndices)
			{
				clusters[idx].childClusterIndices.emplace_back(i);
			}
		}

		// 设置QEM误差
		for (size_t i = 0; i < oldClusterGroups.size(); ++i)
		{
			for (const auto& newClusterIndex : newClusterIndicesSet[i])
			{
				clusters[newClusterIndex].qemError = oldClusterGroups[i].qemError;
			}
		}

		// 计算包围球和表面积
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

		// 计算LOD误差
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
			cluster.lodError = cluster.qemError / (parentCount + 1) + cluster.childLODErrorMax;
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

		// 标记边界顶点
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

		// 构建对偶图
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

		for (auto& cluster : clusters)
		{
			getBoundingSphere(cluster);
			calcSurfaceArea(cluster);
		}
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

			// 标记顶点
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

		// 找最远点py
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

		// 找离py最远的点pz
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

		// 扩展包围球以包含所有点
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
			std::iota(clusterGroupIndex.begin(), clusterGroupIndex.end(), 0);
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
}