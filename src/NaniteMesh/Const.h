#pragma once
#include <unordered_map>
#include "glm/glm.hpp"
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include "metis.h"

namespace Nanite
{
	constexpr int CLUSTER_TARGET_SIZE = 56;
	constexpr int CLUSTER_MAX_SIZE = 64;
	constexpr int CLUSTER_GROUP_TARGET_SIZE = 15;
	constexpr int CLUSTER_GROUP_MAX_SIZE = 32;

	class Graph
	{
	public:
		std::vector<std::unordered_map<uint32_t, int>> adjMap;

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
	
	struct BVHNodeInfo {
		BVHNodeInfo() : pMinWorld(FLT_MAX), pMaxWorld(-FLT_MAX), childrenNodeIndices(-1), errorWorld(FLT_MAX), clusterIntervals(-1)
		{
			//for (size_t i = 0; i < CLUSTER_GROUP_MAX_SIZE; i++)
			//{
			//    clusterIndices[i] = -1;
			//}
		}
		alignas(16) glm::ivec2 clusterIntervals = glm::ivec2(-1, -1); // [clusterIntervals.x, clusterIntervals.y) is the range of clusterIndices
		alignas(16) glm::vec3 pMinWorld = glm::vec3(FLT_MAX);
		alignas(16) glm::vec3 pMaxWorld = glm::vec3(-FLT_MAX);
		alignas(4) uint32_t objectId;
		alignas(16)  glm::vec4 errorR;//node error and parent error. Node error should be non-neccessary, kept for now
		alignas(16)  glm::vec4 errorRP;//node error and parent error. Node error should be non-neccessary, kept for now
		alignas(8)  glm::vec2 errorWorld;//node error and parent error. Node error should be non-neccessary, kept for now
		alignas(16) glm::ivec4 childrenNodeIndices;
		// Note: The annotated one is wrong!
		//alignas(CLUSTER_GROUP_MAX_SIZE * 4) int clusterIndices[CLUSTER_GROUP_MAX_SIZE];
		//alignas(16) int clusterIndices[CLUSTER_GROUP_MAX_SIZE]; // if clusterIndices[0] == -1, then this node is not a leaf node
	};
	
	struct ClusterInfo {
		alignas(16) glm::vec3 pMinWorld = glm::vec3(FLT_MAX);
		alignas(16) glm::vec3 pMaxWorld = glm::vec3(-FLT_MAX);
		// [triangleIndicesStart, triangleIndicesEnd) is the range of triangleIndicesSortedByClusterIdx
		// left close, right open
		alignas(4) uint32_t triangleIndicesStart; // Used to index Mesh::triangleIndicesSortedByClusterIdx
		alignas(4) uint32_t triangleIndicesEnd; // Used to index Mesh::triangleIndicesSortedByClusterIdx
		alignas(4) uint32_t objectIdx;

		void mergeAABB(const glm::vec3& pMinOther, const glm::vec3& pMaxOther) {
			pMinWorld = glm::min(pMinWorld, pMinOther);
			pMaxWorld = glm::max(pMaxWorld, pMaxOther);
		};
	};

	struct ErrorInfo
	{
		alignas(8)  glm::vec2 errorWorld;//node error and parent error in world
		alignas(16) glm::vec4 centerR;
		alignas(16) glm::vec4 centerRP;
	};
	
}
