#include "NaniteLodMesh.h"

#include <unordered_set>

#include "Cluster.h"
#include "ClusterGroup.h"
#include "../utils.h"
#include "../vksTools.h"
#include "metis.h"

namespace Nanite
{
	void NaniteLodMesh::assignTriangleClusterGroup(NaniteLodMesh& lastLOD)
	{
		for (int i = 0; i < lastLOD.clusterGroups.size(); i++)
		{
			oldClusterGroups[i].clusterIndices = lastLOD.clusterGroups[i].clusterIndices;
			oldClusterGroups[i].qemError = lastLOD.clusterGroups[i].qemError;
		}
		for (const auto& heh : mesh.halfedges())
		{
			if (mesh.is_boundary(heh)) continue;
			auto clusterGroupIdx = mesh.property(clusterGroupIndexPropHandle, heh) - 1;
			oldClusterGroups[clusterGroupIdx].clusterGroupHalfedges.push_back(heh);
			oldClusterGroups[clusterGroupIdx].clusterGroupFaces.insert(mesh.face_handle(heh));
		}

		triangleClusterIndex.resize(mesh.n_faces(), -1);
		uint32_t clusterIndexOffset = 0;
		std::vector<std::unordered_set<uint32_t>> newClusterIndicesSet;
		newClusterIndicesSet.resize(oldClusterGroups.size());
		for (size_t i = 0; i < oldClusterGroups.size(); i++)
		{
			auto& oldClusterGroup = oldClusterGroups[i];
			oldClusterGroup.clusterGroupIndexPropHandle = clusterGroupIndexPropHandle;
			oldClusterGroup.mesh = &mesh;
			oldClusterGroup.buildTriangleIndicesLocalGlobalMapping();
			oldClusterGroup.buildLocalTriangleGraph();
			oldClusterGroup.generateLocalClusters();

			for (const auto& fh : oldClusterGroup.clusterGroupFaces)
			{
				auto localTriangleIdx = oldClusterGroup.triangleIndicesGlobalLocalMap[fh.idx()];
				NaniteAssert(triangleClusterIndex[fh.idx()] < 0, "Repeat clsutering");
				uint32_t clusterIdx = clusterIndexOffset + oldClusterGroup.localTriangleClusterIndices[localTriangleIdx];
				triangleClusterIndex[fh.idx()] = clusterIdx;
				newClusterIndicesSet[i].emplace(clusterIdx);
			}
			std::vector<uint32_t> newClusterIndices(newClusterIndicesSet[i].begin(), newClusterIndicesSet[i].end());
			for (auto idx : oldClusterGroup.clusterIndices)
			{
				lastLOD.clusters[idx].parentClusterIndices = newClusterIndices;
			}
			clusterIndexOffset += oldClusterGroup.localClusterNum;
		}


		for (size_t i = 0; i < triangleClusterIndex.size(); i++)
		{
			NaniteAssert(triangleClusterIndex[i] >= 0, "triangleClusterIndex[i] < 0");
		}

		clusterNum = *std::max_element(triangleClusterIndex.begin(), triangleClusterIndex.end()) + 1;
		triangleIndicesSortedByClusterIdx.resize(mesh.n_faces());
		for (size_t i = 0; i < mesh.n_faces(); i++)
			triangleIndicesSortedByClusterIdx[i] = i;

		std::sort(triangleIndicesSortedByClusterIdx.begin(), triangleIndicesSortedByClusterIdx.end(), [&](uint32_t a, uint32_t b)
		{
			return triangleClusterIndex[a] < triangleClusterIndex[b];
		});

		triangleVertexIndicesSortedByClusterIdx.resize(mesh.n_faces() * 3);
		for (int i = 0; i < triangleIndicesSortedByClusterIdx.size(); ++i)
		{
			auto triangleIndex = triangleIndicesSortedByClusterIdx[i];
			auto face = mesh.face_handle(triangleIndex);
			auto fv_it = mesh.fv_iter(face);
			triangleVertexIndicesSortedByClusterIdx[i * 3] = fv_it->idx();
			++fv_it;
			triangleVertexIndicesSortedByClusterIdx[i * 3 + 1] = fv_it->idx();
			++fv_it;
			triangleVertexIndicesSortedByClusterIdx[i * 3 + 2] = fv_it->idx();
		}
		clusters.resize(clusterNum);
		for (NaniteTriMesh::FaceIter face_it = mesh.faces_begin(); face_it != mesh.faces_end(); ++face_it)
		{
			NaniteTriMesh::FaceHandle fh = *face_it;
			auto clusterIdx = triangleClusterIndex[fh.idx()];
			auto& cluster = clusters[clusterIdx];
			cluster.triangleIndices.push_back(fh.idx());
		}
		for (int i = 0; i < lastLOD.clusters.size(); i++)
		{
			for (int idx : lastLOD.clusters[i].parentClusterIndices)
			{
				clusters[idx].childClusterIndices.emplace_back(i);
			}
		}
		for (size_t i = 0; i < oldClusterGroups.size(); i++)
		{
			auto& oldClusterGroup = oldClusterGroups[i];
			auto& newClusterIndices = newClusterIndicesSet[i];
			for (const auto& newClusterIndex : newClusterIndices)
			{
				clusters[newClusterIndex].qemError = oldClusterGroup.qemError;
			}
		}
		for (auto& cluster : clusters)
		{
			calcBoundingSphereFromChildren(cluster, lastLOD);
			calcSurfaceArea(cluster);
			for (int idx : cluster.childClusterIndices)
			{
				auto& childCluster = lastLOD.clusters[idx];
				cluster.boundingSphereRadius = glm::max(cluster.boundingSphereRadius, childCluster.boundingSphereRadius * 2.0f);
			}
		}

		for (auto& cluster : clusters)
		{
			cluster.lodLevel = lodLevel;
			double maxChildNormalizedError = 0.0;
			for (int idx : cluster.childClusterIndices)
			{
				const auto& childCluster = lastLOD.clusters[idx];
				cluster.childLODErrorMax = std::max(cluster.childLODErrorMax, childCluster.lodError);
				maxChildNormalizedError = std::max(maxChildNormalizedError, childCluster.normalizedlodError);
			}
			cluster.childLODErrorMax = std::max(cluster.childLODErrorMax, .0);
			NaniteAssert(cluster.childLODErrorMax >= 0, "cluster.childMaxLODError < 0");
			NaniteAssert(cluster.qemError >= 0, "cluster.qemError < 0");
			cluster.lodError = cluster.qemError / (lastLOD.clusters[cluster.childClusterIndices[0]].parentClusterIndices.size() + 1) + cluster.childLODErrorMax;
			cluster.normalizedlodError = std::max(maxChildNormalizedError + 1e-9, cluster.lodError / (cluster.boundingSphereRadius * cluster.boundingSphereRadius));
			for (int idx : cluster.childClusterIndices)
			{
				auto& childCluster = lastLOD.clusters[idx];
				NaniteAssert(childCluster.parentNormalizedError < 0 || abs(childCluster.parentNormalizedError - cluster.normalizedlodError) < FLT_EPSILON, "Parents have different lod error");
				NaniteAssert(cluster.surfaceArea > DBL_EPSILON, "cluster.surfaceArea <= 0");
				childCluster.parentNormalizedError = cluster.normalizedlodError;
				childCluster.parentSurfaceArea = cluster.surfaceArea;
			}
		}
	}

	void NaniteLodMesh::buildTriangleGraph()
	{
		// 构建对偶图
		int embeddingSize = targetClusterSize * (1 + (mesh.n_faces() + 1) / targetClusterSize) - mesh.n_faces();
		triangleGraph.resize(mesh.n_faces() + embeddingSize);
		isLastLODEdgeVertices.resize(mesh.n_faces() * 3, false);
		for (const NaniteTriMesh::EdgeHandle& edge : mesh.edges())
		{
			NaniteTriMesh::HalfedgeHandle heh = mesh.halfedge_handle(edge, 0);
			NaniteTriMesh::FaceHandle fh = mesh.face_handle(heh);
			NaniteTriMesh::FaceHandle fh2 = mesh.opposite_face_handle(heh);
			auto clusterGroupIdx1 = mesh.property(clusterGroupIndexPropHandle, heh) - 1;
			auto clusterGroupIdx2 = mesh.property(clusterGroupIndexPropHandle, mesh.opposite_halfedge_handle(heh)) - 1;
			if (clusterGroupIdx1 != clusterGroupIdx2)
			{
				auto vh1 = mesh.to_vertex_handle(heh);
				auto vh2 = mesh.to_vertex_handle(mesh.opposite_halfedge_handle(heh));
				// 边界边的两个端点标记为 islastLodEdgeVertices
				for (NaniteTriMesh::FaceVertexIter fv_it = mesh.cfv_iter(fh); fv_it.is_valid(); ++fv_it)
				{
					NaniteTriMesh::VertexHandle vertex = *fv_it;
					if (vertex == vh1 || vertex == vh2)
					{
						isLastLODEdgeVertices[vertex.idx()] = true;
					}
				}
			}
		}

		for (const NaniteTriMesh::EdgeHandle& edge : mesh.edges())
		{
			NaniteTriMesh::HalfedgeHandle heh = mesh.halfedge_handle(edge, 0);
			NaniteTriMesh::FaceHandle fh = mesh.face_handle(heh);
			NaniteTriMesh::FaceHandle fh2 = mesh.opposite_face_handle(heh);
			if (fh.idx() < 0 || fh2.idx() < 0) continue;
			triangleGraph.addEdge(fh.idx(), fh2.idx(), 1);
			triangleGraph.addEdge(fh2.idx(), fh.idx(), 1);
		}
	}

	void NaniteLodMesh::generateCluster()
	{
		MetisGraph triangleMetisGraph = MetisGraph::GraphToMetisGraph(triangleGraph);

		triangleClusterIndex.resize(triangleMetisGraph.nvtxs);
		idx_t ncon = 1;

		int clusterSize = std::min(targetClusterSize, triangleMetisGraph.nvtxs);

		// 生成cluster
		clusterNum = triangleMetisGraph.nvtxs / clusterSize;
		//clusterNum = (triangleMetisGraph.nvtxs + clusterSize - 1) / clusterSize;
		if (clusterNum == 1)
		{
		}
		// Set fixed target cluster size
		auto tpwgts = static_cast<real_t*>(malloc(ncon * clusterNum * sizeof(real_t)));
		float sum = 0;
		for (idx_t i = 0; i < clusterNum; ++i)
		{
			tpwgts[i] = 1.0f / clusterNum; // 
			sum += tpwgts[i];
		}

		idx_t objVal;
		idx_t options[METIS_NOPTIONS];
		METIS_SetDefaultOptions(options);
		options[METIS_OPTION_SEED] = 42; // Set your desired seed value
		auto res = METIS_PartGraphKway(&triangleMetisGraph.nvtxs, &ncon, triangleMetisGraph.xadj.data(), triangleMetisGraph.adjncy.data(), nullptr, nullptr, triangleMetisGraph.adjwgt.data(), &clusterNum, tpwgts, nullptr, options, &objVal, triangleClusterIndex.data());
		free(tpwgts);
		NaniteAssert(res, "METIS_PartGraphKway failed");

		triangleIndicesSortedByClusterIdx.resize(mesh.n_faces());
		for (size_t i = 0; i < mesh.n_faces(); i++)
			triangleIndicesSortedByClusterIdx[i] = i;

		std::sort(triangleIndicesSortedByClusterIdx.begin(), triangleIndicesSortedByClusterIdx.end(), [&](uint32_t a, uint32_t b)
		{
			return triangleClusterIndex[a] < triangleClusterIndex[b];
		});

		triangleVertexIndicesSortedByClusterIdx.resize(mesh.n_faces() * 3);
		for (int i = 0; i < triangleIndicesSortedByClusterIdx.size(); ++i)
		{
			auto triangleIndex = triangleIndicesSortedByClusterIdx[i];
			auto face = mesh.face_handle(triangleIndex);
			auto fv_it = mesh.fv_iter(face);
			triangleVertexIndicesSortedByClusterIdx[i * 3] = fv_it->idx();
			++fv_it;
			triangleVertexIndicesSortedByClusterIdx[i * 3 + 1] = fv_it->idx();
			++fv_it;
			triangleVertexIndicesSortedByClusterIdx[i * 3 + 2] = fv_it->idx();
		}
		clusters.resize(clusterNum);
		for (NaniteTriMesh::FaceIter face_it = mesh.faces_begin(); face_it != mesh.faces_end(); ++face_it)
		{
			NaniteTriMesh::FaceHandle fh = *face_it;
			auto clusterIdx = triangleClusterIndex[fh.idx()];
			auto& cluster = clusters[clusterIdx];
			cluster.triangleIndices.push_back(fh.idx());
			cluster.lodLevel = lodLevel;
			cluster.lodError = -1;
		}

		for (auto& cluster : clusters)
		{
			getBoundingSphere(cluster);
			calcSurfaceArea(cluster);
		}
	}


	void NaniteLodMesh::buildClusterGraph()
	{
		int embeddedSize = (clusterNum + targetClusterGroupSize - 1) / targetClusterGroupSize * targetClusterGroupSize;
		clusterGraph.resize(embeddedSize);

		for (const NaniteTriMesh::EdgeHandle& edge : mesh.edges())
		{
			NaniteTriMesh::HalfedgeHandle heh = mesh.halfedge_handle(edge, 0);
			NaniteTriMesh::FaceHandle fh = mesh.face_handle(heh);
			NaniteTriMesh::FaceHandle fh2 = mesh.opposite_face_handle(heh);
			if (fh.idx() < 0 || fh2.idx() < 0) continue;
			auto clusterIdx1 = triangleClusterIndex[fh.idx()];
			auto clusterIdx2 = triangleClusterIndex[fh2.idx()];
			if (clusterIdx1 != clusterIdx2)
			{
				clusterGraph.addEdgeCost(clusterIdx1, clusterIdx2, 1);
				clusterGraph.addEdgeCost(clusterIdx2, clusterIdx1, 1);
			}
		}
	}

	void NaniteLodMesh::colorClusterGraph()
	{
		std::vector<int> clusterSortedByConnectivity(clusterNum, -1);
		for (size_t i = 0; i < clusterNum; i++)
		{
			clusterSortedByConnectivity[i] = i;
		}
		std::sort(clusterSortedByConnectivity.begin(), clusterSortedByConnectivity.end(), [&](int a, int b)
		{
			return clusterGraph.adjMap[a].size() > clusterGraph.adjMap[b].size();
		});

		for (int clusterIndex : clusterSortedByConnectivity)
		{
			std::unordered_set<int> neighbor_colors;
			for (auto tosAndCosts : clusterGraph.adjMap[clusterIndex])
			{
				auto neighbor = tosAndCosts.first;
				if (clusterColorAssignment.contains(neighbor))
				{
					neighbor_colors.insert(clusterColorAssignment[neighbor]);
				}
			}

			int color = 0;
			while (neighbor_colors.contains(color))
			{
				color++;
			}

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

		size_t original_faces = mymesh.n_faces();
		std::cout << "NUM FACES BEFORE: " << original_faces << std::endl;
		constexpr double percentage = 0.5;

		auto currTargetFaceNum = mymesh.n_faces();
		for (uint32_t i = 0; i < clusterGroups.size(); ++i)
		{
			currTargetFaceNum = mymesh.n_faces() - clusterGroups[i].localFaceNum * (1.0f - percentage);
			NaniteTriMesh submesh;
			uint32_t faceCount = 0;
			for (const auto& heh : mymesh.halfedges())
			{
				auto clusterGroupIdx = mymesh.property(clusterGroupIndexPropHandle, heh) - 1;
				if (clusterGroupIdx == i)
				{
					auto vh1 = mymesh.to_vertex_handle(heh);
					auto vh2 = mymesh.from_vertex_handle(heh);
					auto vh3 = mymesh.to_vertex_handle(mymesh.next_halfedge_handle(heh));
					std::vector<NaniteTriMesh::VertexHandle> face_vhandles;
					auto vh1_new = submesh.add_vertex(mymesh.point(vh1));
					auto vh2_new = submesh.add_vertex(mymesh.point(vh2));
					auto vh3_new = submesh.add_vertex(mymesh.point(vh3));
					submesh.add_face(vh1_new, vh2_new, vh3_new);
					faceCount++;
					mymesh.status(vh1).set_selected(true);
					mymesh.status(vh2).set_selected(true);
					if (mymesh.is_boundary(mymesh.opposite_halfedge_handle(heh))
						|| (mymesh.property(clusterGroupIndexPropHandle, mymesh.opposite_halfedge_handle(heh)) - 1) != i)
					{
						mymesh.status(vh1).set_locked(true);
						mymesh.status(vh2).set_locked(true);
					}
				}
			}

			std::cout << "Cluster group: " << i << " Face num: " << currTargetFaceNum << std::endl;

			auto n_collapses = decimater.decimate_to_faces(0, currTargetFaceNum, true);
			std::cout << "Total error: " << decimater.module(hModQuadric).total_err() << std::endl;
			clusterGroups[i].qemError = decimater.module(hModQuadric).total_err();
			decimater.module(hModQuadric).clear_total_err();
			std::cout << "n_collapses: " << n_collapses << std::endl;
			mymesh.garbage_collection();
			for (const auto& vh : mymesh.vertices())
			{
				mymesh.status(vh).set_selected(false);
			}

			std::cout << "NUM FACES AFTER: " << mymesh.n_faces() << std::endl;
		}
		//std::cout << "Curr cluster group num: " << clusterGroups.size() << std::endl;
		for (const auto& vh : mymesh.vertices())
		{
			mymesh.status(vh).set_selected(false);
			mymesh.status(vh).set_locked(false);
		}
		size_t target_faces = static_cast<size_t>(original_faces * percentage);

		mymesh.garbage_collection();
		size_t actual_faces = mymesh.n_faces();
		std::cout << "NUM FACES AFTER: " << actual_faces << std::endl;
	}

	void NaniteLodMesh::calcBoundingSphereFromChildren(Cluster& cluster, NaniteLodMesh& lastLOD)
	{
		auto center = glm::vec3(0);
		float max_radius = 0.0;
		for (auto& i : cluster.childClusterIndices)
		{
			auto& childCluster = lastLOD.clusters[i];
			center += childCluster.boundingSphereCenter;
			max_radius = std::max(max_radius, childCluster.boundingSphereRadius);
		}
		center /= cluster.childClusterIndices.size();
		cluster.boundingSphereCenter = center;
		cluster.boundingSphereRadius = max_radius;
	}

	void NaniteLodMesh::getBoundingSphere(Cluster& cluster)
	{
		//std::cout << "Cluster: " << cluster.triangleIndices.size() << std::endl;
		if (cluster.triangleIndices.size() == 0)
		{
			//std::cout << "Empty cluster" << std::endl;
			cluster.boundingSphereCenter = glm::vec3(0.0f);
			cluster.boundingSphereRadius = 0.0f;
			return;
		}
		const auto& triangleIndices = cluster.triangleIndices;
		auto px = *mesh.fv_begin(mesh.face_handle(triangleIndices[0]));
		NaniteTriMesh::VertexHandle py, pz;
		float dist2_max = -1;
		for (const auto triangleIndex : triangleIndices)
		{
			auto face = mesh.face_handle(triangleIndex);
			auto fv_it = mesh.fv_iter(face);
			auto vx = *fv_it;
			++fv_it;
			auto vy = *fv_it;
			++fv_it;
			auto vz = *fv_it;
			for (const auto& vh : {vx, vy, vz})
			{
				float dist2 = (mesh.point(vh) - mesh.point(px)).sqrnorm();
				if (dist2 > dist2_max)
				{
					dist2_max = dist2;
					py = vh;
				}
			}
		}
		dist2_max = -1;
		for (const auto triangleIndex : triangleIndices)
		{
			auto face = mesh.face_handle(triangleIndex);
			auto fv_it = mesh.fv_iter(face);
			auto vx = *fv_it;
			++fv_it;
			auto vy = *fv_it;
			++fv_it;
			auto vz = *fv_it;
			for (const auto& vh : {vx, vy, vz})
			{
				float dist2 = (mesh.point(vh) - mesh.point(py)).sqrnorm();
				if (dist2 > dist2_max)
				{
					dist2_max = dist2;
					pz = vh;
				}
			}
		}

		auto c = (mesh.point(py) + mesh.point(pz)) / 2.0f;
		auto r = sqrt(dist2_max) / 2.0f;

		for (const auto triangleIndex : triangleIndices)
		{
			auto face = mesh.face_handle(triangleIndex);
			auto fv_it = mesh.fv_iter(face);
			auto vx = *fv_it;
			++fv_it;
			auto vy = *fv_it;
			++fv_it;
			auto vz = *fv_it;
			for (const auto& vh : {vx, vy, vz})
			{
				if ((mesh.point(vh) - c).sqrnorm() > r * r)
				{
					r = sqrt((mesh.point(vh) - c).sqrnorm());
				}
			}
		}

		cluster.boundingSphereCenter = glm::vec3(c[0], c[1], c[2]);
		cluster.boundingSphereRadius = r;
		NaniteAssert(cluster.boundingSphereRadius > 0, "cluster.boundingSphereRadius <= 0");
	}

	void NaniteLodMesh::calcSurfaceArea(Cluster& cluster)
	{
		cluster.surfaceArea = 0.0f;
		for (const auto triangleIndex : cluster.triangleIndices)
		{
			auto face = mesh.face_handle(triangleIndex);
			auto fv_it = mesh.fv_iter(face);
			auto vx = *fv_it;
			++fv_it;
			auto vy = *fv_it;
			++fv_it;
			auto vz = *fv_it;
			cluster.surfaceArea += mesh.calc_face_area(face);
		}
	}

	nlohmann::json NaniteLodMesh::toJson()
	{
		nlohmann::json result = {
			{"clusterNum", clusterNum},
			{"triangleClusterIndex", triangleClusterIndex},
			{"clusterColorAssignment", clusterColorAssignment},
			{"clusterGroupIndex", clusterGroupIndex},
			{"triangleIndicesSortedByClusterIdx", triangleIndicesSortedByClusterIdx},
			{"triangleVertexIndicesSortedByClusterIdx", triangleVertexIndicesSortedByClusterIdx}
		};

		for (size_t i = 0; i < clusters.size(); i++)
		{
			result["clusters"].push_back(clusters[i].toJson());
		}
		return result;
	}

	void NaniteLodMesh::fromJson(const nlohmann::json& j)
	{
		clusterNum = j["clusterNum"].get<int>();
		clusterColorAssignment = j["clusterColorAssignment"].get<std::unordered_map<int, int>>();
		triangleClusterIndex = j["triangleClusterIndex"].get<std::vector<int>>();
		clusterGroupIndex = j["clusterGroupIndex"].get<std::vector<int>>();
		triangleIndicesSortedByClusterIdx = j["triangleIndicesSortedByClusterIdx"].get<std::vector<uint32_t>>();
		triangleVertexIndicesSortedByClusterIdx = j["triangleVertexIndicesSortedByClusterIdx"].get<std::vector<uint32_t>>();

		clusters.resize(clusterNum);
		for (size_t i = 0; i < clusters.size(); i++)
		{
			clusters[i].fromJson(j["clusters"][i]);
		}
	}


	void NaniteLodMesh::generateClusterGroup()
	{
		MetisGraph clusterMetisGraph = MetisGraph::GraphToMetisGraph(clusterGraph);
		clusterGroupIndex.resize(clusterMetisGraph.nvtxs);

		idx_t ncon = 1;
		clusterGroupNum = clusterMetisGraph.nvtxs / targetClusterGroupSize;
		clusterGroups.resize(clusterGroupNum);
		if (clusterGroupNum == 1)
		{
			// Should quit now
			for (size_t i = 0; i < clusterMetisGraph.nvtxs; i++)
			{
				clusterGroupIndex[i] = 0;
				clusterGroups[0].clusterIndices.push_back(i);
			}
			for (auto& heh : mesh.halfedges())
			{
				if (mesh.is_boundary(heh)) continue;
				mesh.property(clusterGroupIndexPropHandle, heh) = 1;
			}
			return;
		}

		auto tpwgts = static_cast<real_t*>(malloc(ncon * clusterGroupNum * sizeof(real_t)));
		for (idx_t i = 0; i < clusterGroupNum; ++i)
		{
			tpwgts[i] = static_cast<float>(targetClusterGroupSize) / clusterMetisGraph.nvtxs;
		}

		idx_t objVal;
		idx_t options[METIS_NOPTIONS];
		METIS_SetDefaultOptions(options);
		options[METIS_OPTION_SEED] = 42;
		auto res = METIS_PartGraphKway(&clusterMetisGraph.nvtxs, &ncon, clusterMetisGraph.xadj.data(), clusterMetisGraph.adjncy.data(), nullptr, nullptr, clusterMetisGraph.adjwgt.data(), &clusterGroupNum, tpwgts, nullptr, options, &objVal, clusterGroupIndex.data());
		free(tpwgts);
		NaniteAssert(res, "METIS_PartGraphKway failed");

		for (size_t clusterIdx = 0; clusterIdx < clusterNum; clusterIdx++)
		{
			auto cluster = clusters[clusterIdx];
			auto clusterGroupIdx = clusterGroupIndex[clusterIdx];

			cluster.clusterGroupIndex = clusterGroupIdx;
			clusterGroups[clusterGroupIdx].clusterIndices.push_back(clusterIdx);
		}

		std::cout << std::count(clusterGroupIndex.begin(), clusterGroupIndex.end(), 0) << std::endl;

		isEdgeVertices.resize(mesh.n_faces() * 3, false);
		std::vector<std::unordered_set<NaniteTriMesh::FaceHandle>> clusterGroupFaceHandles;
		clusterGroupFaceHandles.resize(clusterGroupNum);
		for (const NaniteTriMesh::EdgeHandle& edge : mesh.edges())
		{
			NaniteTriMesh::HalfedgeHandle heh = mesh.halfedge_handle(edge, 0);
			NaniteTriMesh::FaceHandle fh = mesh.face_handle(heh);
			NaniteTriMesh::FaceHandle fh2 = mesh.opposite_face_handle(heh);
			if (mesh.is_boundary(mesh.opposite_halfedge_handle(heh)) && fh.idx() >= 0)
			{
				auto clusterIdx1 = triangleClusterIndex[fh.idx()];
				auto clusterGroupIdx1 = clusterGroupIndex[clusterIdx1];
				auto vh1 = mesh.to_vertex_handle(heh);
				auto vh2 = mesh.from_vertex_handle(heh);
				mesh.property(clusterGroupIndexPropHandle, heh) = clusterGroupIdx1 + 1;
				clusterGroupFaceHandles[clusterGroupIdx1].insert(fh);
				continue;
			}
			if (mesh.is_boundary(heh) && fh2.idx() >= 0)
			{
				auto clusterIdx2 = triangleClusterIndex[fh2.idx()];
				auto clusterGroupIdx2 = clusterGroupIndex[clusterIdx2];
				auto vh1 = mesh.to_vertex_handle(mesh.opposite_halfedge_handle(heh));
				auto vh2 = mesh.from_vertex_handle(mesh.opposite_halfedge_handle(heh));
				mesh.property(clusterGroupIndexPropHandle, heh) = clusterGroupIdx2 + 1;
				clusterGroupFaceHandles[clusterGroupIdx2].insert(fh2);
				continue;
			}
			if (fh.idx() < 0 || fh2.idx() < 0) continue;
			auto clusterIdx1 = triangleClusterIndex[fh.idx()];
			auto clusterIdx2 = triangleClusterIndex[fh2.idx()];
			auto clusterGroupIdx1 = clusterGroupIndex[clusterIdx1];
			auto clusterGroupIdx2 = clusterGroupIndex[clusterIdx2];
			clusterGroupFaceHandles[clusterGroupIdx1].insert(fh);
			clusterGroupFaceHandles[clusterGroupIdx2].insert(fh2);
			mesh.property(clusterGroupIndexPropHandle, heh) = clusterGroupIdx1 + 1;
			mesh.property(clusterGroupIndexPropHandle, mesh.opposite_halfedge_handle(heh)) = clusterGroupIdx2 + 1;
		}

		for (size_t i = 0; i < clusterGroupNum; i++)
		{
			clusterGroups[i].localFaceNum = clusterGroupFaceHandles[i].size();
		}
	}

	void NaniteLodMesh::initVertexBuffer()
	{
		for (NaniteTriMesh::FaceIter f_it = mesh.faces_begin(); f_it != mesh.faces_end(); ++f_it)
		{
			NaniteTriMesh::FaceHandle face = *f_it;
			for (NaniteTriMesh::FaceVertexIter fv_it = mesh.cfv_iter(face); fv_it.is_valid(); ++fv_it)
			{
				NaniteTriMesh::VertexHandle vertex = *fv_it;
				vkglTF::Vertex v;
				v.pos = glm::vec3(mesh.point(vertex)[0], mesh.point(vertex)[1], mesh.point(vertex)[2]);
				v.normal = glm::vec3(mesh.normal(vertex)[0], mesh.normal(vertex)[1], mesh.normal(vertex)[2]);
				v.uv = glm::vec2(mesh.texcoord2D(vertex)[0], mesh.texcoord2D(vertex)[1]);
				int clusterId = triangleClusterIndex[face.idx()];
				v.joint0 = glm::vec4(nodeColors[clusterColorAssignment[clusterId]], clusterId);

				vertexBuffer.push_back(v);
			}
		}
	}
	
	void NaniteLodMesh::initUniqueVertexBuffer()
	{
		for (const auto& vertex : mesh.vertices())
		{
			vkglTF::Vertex v;
			v.pos = glm::vec3(mesh.point(vertex)[0], mesh.point(vertex)[1], mesh.point(vertex)[2]);
			v.normal = glm::vec3(mesh.normal(vertex)[0], mesh.normal(vertex)[1], mesh.normal(vertex)[2]);
			v.uv = glm::vec2(mesh.texcoord2D(vertex)[0], mesh.texcoord2D(vertex)[1]);
			v.joint0 = glm::vec4(lodLevel);
			v.weight0 = glm::vec4(0.0f);
			uniqueVertexBuffer.emplace_back(v);
		}
	}


	void NaniteLodMesh::createVertexBuffer(VulkanExampleBase& variableLink)
	{
		size_t vertexBufferSize = vertexBuffer.size() * sizeof(vkglTF::Vertex);
		vertices.count = static_cast<uint32_t>(vertexBuffer.size());
		vks::vksTools::createStagingBuffer(variableLink, 0, vertexBufferSize, vertexBuffer.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices);
	}
	
}
