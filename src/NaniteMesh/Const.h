#pragma once
#include <unordered_map>
#include "glm/glm.hpp"
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include "metis.h"

namespace Nanite
{
	constexpr int CLUSTER_SIZE = 56;
	constexpr int CLUSTER_THRESHOLD = 64;
	constexpr int CLUSTER_GROUP_SIZE = 15;
	constexpr int CLUSTER_GROUP_THRESHOLD = 32;

	class Graph
	{
	public:
		std::vector<std::unordered_map<int, uint32_t>> adjMap;

		void resize(uint32_t newSize);
		void addEdge(uint32_t from, uint32_t to, int cost);
		void addEdgeCost(uint32_t from, uint32_t to, int cost);
	};

	class MetisGraph
	{
	public:
		idx_t nvtxs;
		std::vector<idx_t> xadj;
		std::vector<idx_t> adjncy;
		std::vector<idx_t> adjwgt;

		static MetisGraph GraphToMetisGraph(const Graph& graph);
	};

	// 启用normal和texcoord2d
	struct NaniteOpenMeshTraits : OpenMesh::DefaultTraits
	{
		VertexAttributes(OpenMesh::Attributes::Normal | OpenMesh::Attributes::TexCoord2D);
	};

	using NaniteTriMesh = OpenMesh::TriMesh_ArrayKernelT<NaniteOpenMeshTraits>;

	// void TestNaniteTriMesh();
	// {
	//     NaniteTriMesh mesh;
	//     NaniteTriMesh::VertexHandle vh1 = mesh.add_vertex(NaniteTriMesh::Point(0,0,0));
	//
	//     mesh.set_normal(vh1, NaniteTriMesh::Normal(0,0,1));
	//     mesh.set_texcoord2D(vh1, NaniteTriMesh::TexCoord2D(0,0));
	//     std::vector<NaniteTriMesh::VertexHandle> face_handles;
	//     face_handles.emplace_back(vh1);
	//     face_handles.emplace_back(vh1);
	//     face_handles.emplace_back(vh1);
	//
	//     mesh.add_face(face_handles);
	// }

	// 会传入给shader
	class ClusterInfo
	{
	public:
		alignas(16) glm::vec3 pMinWorld = glm::vec3(FLT_MAX);
		alignas(16) glm::vec3 pMaxWorld = glm::vec3(-FLT_MAX);
		alignas(4) uint32_t triangleIndicesStart;
		alignas(4) uint32_t triangleIndicesEnd;
		alignas(4) uint32_t objectIdx;

		void mergeAABB(const glm::vec3& pMin, const glm::vec3& pMax)
		{
			pMinWorld = glm::min(pMinWorld, pMin);
			pMaxWorld = glm::max(pMinWorld, pMax);
		}
	};

	class ErrorInfo
	{
	public:
		alignas(16) glm::vec4 centerR;
		alignas(16) glm::vec4 centerRP;
		alignas(8) glm::vec2 errorWorld;
	};
}
