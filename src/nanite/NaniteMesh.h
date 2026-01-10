#pragma once
#include <tinygltf/tiny_gltf.h>

#include "Const.h"
#include "NaniteLodMesh.h"
#include "VulkanglTFModel.h"

namespace vks
{
	struct VulkanDevice;
}

namespace vkglTF
{
	struct Vertex;
	struct Primitive;
	struct Mesh;
	class Model;
}

namespace Nanite
{
	class NaniteBVHNode;
	class NaniteBVHNodeInfo;
	class ClusterNode;

	class NaniteMesh
	{
	public:
		uint32_t lodNums = 0;
		glm::mat4 modelMatrix;
		std::vector<NaniteLodMesh> meshes;
		OpenMesh::HPropHandleT<int32_t> clusterGroupIndexPropHandle;

		const vkglTF::Model* vkglTFModel;
		const vkglTF::Mesh* vkglTFMesh;
		void setModelPath(const char* path) { filepath = path; };
		void loadvkglTFModel(const vkglTF::Model& model);
		void vkglTFMeshToOpenMesh(NaniteTriMesh& mymesh, const vkglTF::Mesh& mesh);
		void vkglTFPrimitiveToOpenMesh(NaniteTriMesh& mymesh, const vkglTF::Primitive& prim);
		const tinygltf::Model* tinyglTFModel;
		const tinygltf::Mesh* tinyglTFMesh;
		void loadglTFModel(const tinygltf::Model& model);
		void glTFMeshToOpenMesh(NaniteTriMesh& mymesh, const tinygltf::Mesh& mesh);

		/* 暂时用DAG代替 */
		std::vector<ClusterNode> flattenedClusterNodes;
		void flattenDAG();

		// 展平BVH
		std::shared_ptr<NaniteBVHNode> virtualBVHRootNode;
		std::vector<NaniteBVHNodeInfo> flattenedBVHNodeInfos;
		void flattenBVH();

		// 序列化
		void generateNaniteInfo();
		void serialize(const std::string& filepath);
		void deserialize(const std::string& filepath);
		
		std::vector<ClusterInfo> clusterInfo;
		std::vector<ErrorInfo> errorInfo;
		std::vector<uint32_t> sortedClusterIndices;
		void buildClusterInfo();

		void initNaniteInfo(const std::string& filepath, bool useCache = true);
		vks::VulkanDevice* device;
		const vkglTF::Model* model;
		vkglTF::Model::Vertices vertices;
		vkglTF::Model::Indices indices;
		std::vector<uint32_t> indexBuffer;
		std::vector<vkglTF::Vertex> vertexBuffer;
		std::vector<vkglTF::Primitive> primitives;
		
		std::vector<uint32_t> clusterIndexOffset; // 不同LOD层级导致的 cluster 索引偏移

		const char* filepath = nullptr;
		const char* cache_time_key = "cache_time";

		std::vector<NaniteLodMesh> debugMeshes;
		void checkDeserializationResult(const std::string& filepath);

		bool operator==(const NaniteMesh& other) const;
	};
}
