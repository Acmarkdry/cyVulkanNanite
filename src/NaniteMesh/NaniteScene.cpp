#include "NaniteScene.h"

#include <algorithm>
#include <numeric>

#include <OpenMesh/Core/IO/MeshIO.hh>
#include "../vksTools.h"

namespace Nanite
{
    size_t NaniteScene::calculateTotalVertexCount() const
    {
        return std::accumulate(naniteMeshes.begin(), naniteMeshes.end(), size_t{0},
            [](size_t sum, const NaniteMesh& mesh) {
                return sum + std::accumulate(mesh.meshes.begin(), mesh.meshes.end(), size_t{0},
                    [](size_t s, const NaniteLodMesh& lod) {
                        return s + lod.uniqueVertexBuffer.size();
                    });
            });
    }

    size_t NaniteScene::calculateTotalIndexCount() const
    {
        return std::accumulate(naniteMeshes.begin(), naniteMeshes.end(), size_t{0},
            [](size_t sum, const NaniteMesh& mesh) {
                return sum + std::accumulate(mesh.meshes.begin(), mesh.meshes.end(), size_t{0},
                    [](size_t s, const NaniteLodMesh& lod) {
                        return s + lod.triangleVertexIndicesSortedByClusterIdx.size();
                    });
            });
    }

    ptrdiff_t NaniteScene::findMeshIndex(const NaniteMesh& mesh) const
    {
        auto it = std::find(naniteMeshes.begin(), naniteMeshes.end(), mesh);
        return (it != naniteMeshes.end()) ? std::distance(naniteMeshes.begin(), it) : -1;
    }

    void NaniteScene::createVertexIndexBuffer(VulkanExampleBase& link)
    {
        // 预分配容量
        const size_t totalVertexCount = calculateTotalVertexCount();
        const size_t totalIndexCount = calculateTotalIndexCount();

        std::vector<vkglTF::Vertex> vertexBuffer;
        std::vector<uint32_t> indexBuffer;
        vertexBuffer.reserve(totalVertexCount);
        indexBuffer.reserve(totalIndexCount);

        const auto meshCount = naniteMeshes.size();
        indexOffsets.resize(meshCount);
        indexCounts.resize(meshCount);

        uint32_t indexOffset = 0;

        for (size_t i = 0; i < meshCount; ++i)
        {
            // 使用引用避免拷贝整个NaniteMesh
            auto instance = NaniteInstance(&naniteMeshes[i], glm::mat4(1.0f));
            instance.initBufferForNaniteLODs();

            // 批量插入顶点
            vertexBuffer.insert(vertexBuffer.end(),
                instance.vertexBuffer.begin(),
                instance.vertexBuffer.end());

            // 添加偏移后的索引
            for (const auto idx : instance.indexBuffer)
            {
                indexBuffer.emplace_back(idx + indexOffset);
            }

            indexOffsets[i] = indexOffset;
            indexCounts[i] = static_cast<uint32_t>(instance.indexBuffer.size());
            indexOffset += static_cast<uint32_t>(instance.vertexBuffer.size());
        }

        // 创建Vulkan缓冲区
        constexpr VkBufferUsageFlags vertexUsage = 
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        constexpr VkBufferUsageFlags indexUsage = 
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        const size_t vertexBufferSize = vertexBuffer.size() * sizeof(vkglTF::Vertex);
        vertices.count = static_cast<uint32_t>(vertexBuffer.size());
        vks::vksTools::createStagingBuffer(link, 0, vertexBufferSize, vertexBuffer.data(), 
            vertexUsage, vertices);

        const size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
        indices.count = static_cast<uint32_t>(indexBuffer.size());
        vks::vksTools::createStagingBuffer(link, 0, indexBufferSize, indexBuffer.data(), 
            indexUsage, indices);
    }

    void NaniteScene::createClusterInfos()
    {
        sceneIndicesCount = 0;
        visibleIndicesCount = 0;

        // 预计算总cluster数量以预分配容量
        size_t totalClusterCount = 0;
        for (const auto& obj : naniteObjects)
        {
            if (obj.referenceMesh != nullptr)
            {
                totalClusterCount += std::accumulate(
                    obj.referenceMesh->meshes.begin(),
                    obj.referenceMesh->meshes.end(),
                    size_t{0},
                    [](size_t sum, const NaniteLodMesh& mesh) {
                        return sum + mesh.clusterNum;
                    });
            }
        }

        clusterInfo.clear();
        clusterInfo.reserve(totalClusterCount);
        errorInfo.clear();
        errorInfo.reserve(totalClusterCount);

        for (size_t i = 0; i < naniteObjects.size(); ++i)
        {
            auto& naniteObject = naniteObjects[i];
            naniteObject.buildClusterInfo();

            const auto referenceMeshIndex = findMeshIndex(*naniteObject.referenceMesh);
            
            // 计算索引偏移量
            uint32_t triangleOffset = 0;
            if (referenceMeshIndex > 0)
            {
                triangleOffset = indexCounts[referenceMeshIndex - 1] / 3;
            }

            // 添加cluster信息
            for (auto ci : naniteObject.clusterInfo)
            {
                ci.triangleIndicesStart += triangleOffset;
                ci.triangleIndicesEnd += triangleOffset;
                ci.objectIdx = static_cast<int>(i);
                clusterInfo.emplace_back(ci);
            }

            // 批量插入error信息
            errorInfo.insert(errorInfo.end(),
                naniteObject.errorInfo.begin(),
                naniteObject.errorInfo.end());

            // 累加计数
            if (referenceMeshIndex >= 0)
            {
                sceneIndicesCount += indexCounts[referenceMeshIndex];
            }
            
            if (!naniteObject.referenceMesh->meshes.empty())
            {
                visibleIndicesCount += static_cast<uint32_t>(
                    naniteObject.referenceMesh->meshes[0].triangleVertexIndicesSortedByClusterIdx.size());
            }
        }
    }
}