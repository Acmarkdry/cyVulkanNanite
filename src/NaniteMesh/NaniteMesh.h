#pragma once
#include <glm/detail/type_mat.hpp>
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
		void loadglTFModel(const tinygltf::Model& model); // TODO
		void glTFMeshToOpenMesh(NaniteTriMesh& mymesh, const tinygltf::Mesh& mesh); // TODO

		/*暂时用dag代替*/
		std::vector<ClusterNode> flattenedClusterNodes;
		void flattenDAG();

		// 序列化
		void generateNaniteInfo();
		void serialize(const std::string& filepath);
		void deserialize(const std::string& filepath);

		void initNaniteInfo(const std::string& filepath, bool useCache = true);
		vks::VulkanDevice* device;
		const vkglTF::Model* model;
		vkglTF::Model::Vertices vertices;
		vkglTF::Model::Indices indices;
		std::vector<uint32_t> indexBuffer;
		std::vector<vkglTF::Vertex> vertexBuffer;
		std::vector<vkglTF::Primitive> primitives;

		const char* filepath = nullptr;
		const char* cache_time_key = "cache_time";

		std::vector<NaniteLodMesh> debugMeshes;
		void checkDeserializationResult(const std::string& filepath);

		bool operator==(const NaniteMesh& other) const;
	};
}
