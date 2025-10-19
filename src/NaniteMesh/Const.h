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

	// 会传入给Shader的BVH节点信息
	struct BVHNodeInfo {
		BVHNodeInfo() : pMinWorld(FLT_MAX), pMaxWorld(-FLT_MAX), childrenNodeIndices(-1), errorWorld(FLT_MAX), clusterIntervals(-1)
		{
		}
		alignas(16) glm::ivec2 clusterIntervals = glm::ivec2(-1, -1); // [clusterIntervals.x, clusterIntervals.y) 为 clusterIndices 的索引范围
		alignas(16) glm::vec3 pMinWorld = glm::vec3(FLT_MAX);
		alignas(16) glm::vec3 pMaxWorld = glm::vec3(-FLT_MAX);
		alignas(4) uint32_t objectId;
		alignas(16) glm::vec4 errorR;    // 节点误差与父节点误差的包围球信息
		alignas(16) glm::vec4 errorRP;   // 父节点误差包围球信息
		alignas(8)  glm::vec2 errorWorld; // 节点误差与父节点误差（世界空间）
		alignas(16) glm::ivec4 childrenNodeIndices;
	};
	
	struct ClusterInfo {
		alignas(16) glm::vec3 pMinWorld = glm::vec3(FLT_MAX);
		alignas(16) glm::vec3 pMaxWorld = glm::vec3(-FLT_MAX);
		// [triangleIndicesStart, triangleIndicesEnd) 为 triangleIndicesSortedByClusterIdx 的索引范围（左闭右开）
		alignas(4) uint32_t triangleIndicesStart; // 用于索引 Mesh::triangleIndicesSortedByClusterIdx
		alignas(4) uint32_t triangleIndicesEnd;   // 用于索引 Mesh::triangleIndicesSortedByClusterIdx
		alignas(4) uint32_t objectIdx;

		void mergeAABB(const glm::vec3& pMinOther, const glm::vec3& pMaxOther) {
			pMinWorld = glm::min(pMinWorld, pMinOther);
			pMaxWorld = glm::max(pMaxWorld, pMaxOther);
		};
	};

	struct ErrorInfo
	{
		alignas(8)  glm::vec2 errorWorld; // 世界空间中的节点误差与父节点误差
		alignas(16) glm::vec4 centerR;
		alignas(16) glm::vec4 centerRP;
	};
	
}
