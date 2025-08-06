#pragma once
#include <glm/detail/type_mat.hpp>
#include <OpenMesh/Core/IO/MeshIO.hh>
#include <tinygltf/tiny_gltf.h>

#include "Const.h"
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
    class NaniteLodMesh;

    class NaniteMesh
    {
    public:
        uint32_t lodNums = 0;
        glm::mat4 modelMatrix;
        std::vector<NaniteLodMesh> meshes;
        
        OpenMesh::HPropHandleT<int32_t> clusterGroupIndexPropHandle;

        /************ Load Mesh *************/
        // Load from vkglTF
        const vkglTF::Model* vkglTFModel;
        const vkglTF::Mesh* vkglTFMesh;
        void setModelPath(const char* path) { filepath = path; };
        void loadvkglTFModel(const vkglTF::Model& model);
        void vkglTFMeshToOpenMesh(NaniteTriMesh& NaniteTriMesh, const vkglTF::Mesh& mesh);
        void vkglTFPrimitiveToOpenMesh(NaniteTriMesh& NaniteTriMesh, const vkglTF::Primitive& prim);
        // TODO: Load from tinygltf
        const tinygltf::Model* tinyglTFModel;
        const tinygltf::Mesh* tinyglTFMesh;
        void loadglTFModel(const tinygltf::Model& model); // TODO
        void glTFMeshToOpenMesh(NaniteTriMesh& NaniteTriMesh, const tinygltf::Mesh& mesh); // TODO

        /************ Flatten BVH *************/
        void flattenBVH();

        /************ Build Info *************/
        void generateNaniteInfo();

        std::vector<ClusterInfo> clusterInfo;
        std::vector<ErrorInfo> errorInfo;
        std::vector<uint32_t> sortedClusterIndices;
        void buildClusterInfo();

        /************ Serialization *************/
        void serialize(const std::string& filepath);
        void deserialize(const std::string& filepath);


        void initNaniteInfo(const std::string& filepath, bool useCache = true);
        /*
            What data structure should be used to store 
            1. lods
            2. cluster of each lods
            3. cluster group (no need)
        
        */
        vks::VulkanDevice* device;
        const vkglTF::Model* model;
        vkglTF::Model::Vertices vertices;
        vkglTF::Model::Indices indices;
        std::vector<uint32_t> indexBuffer;
        std::vector<vkglTF::Vertex> vertexBuffer;
        std::vector<vkglTF::Primitive> primitives;
        std::vector<uint32_t> clusterIndexOffset; // This offset is caused by different LODs

        const char* filepath = nullptr;
        const char* cache_time_key = "cache_time";

        void checkDeserializationResult(const std::string& filepath);

        bool operator==(const NaniteMesh& other) const;
    };
}
