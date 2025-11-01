#include "NaniteScene.h"

#include <algorithm>
#include <numeric>
#include <queue>

#include <OpenMesh/Core/IO/MeshIO.hh>

#include "NaniteBVH.h"
#include "utils.h"
#include "vksTools.h"

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
        // 预分配容�?
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
        	maxLodLevelNum = glm::max(maxLodLevelNum, instance.referenceMesh->lodNums);
        }

        // 创建Vulkan缓冲�?
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

    void NaniteScene::createClusterInfos(VulkanExampleBase& link)
    {
    	sceneIndicesCount = 0;
    	clusterIndexOffsets.resize(naniteMeshes.size());
    	clusterIndexCounts.resize(naniteMeshes.size());
    	for (size_t i = 0; i < naniteMeshes.size(); i++)
    	{
    		auto& naniteMesh = naniteMeshes[i];
    		auto instance = NaniteInstance(&naniteMesh, glm::mat4(1.0f));
    		naniteMesh.buildClusterInfo();
    		clusterIndexOffsets[i] = clusterInfo.size();
    		//clusterInfo.insert(clusterInfo.end(), naniteMesh.clusterInfo.begin(), naniteMesh.clusterInfo.end());
    		for (auto ci : naniteMesh.clusterInfo)
    		{
    			if (i != 0) {
    				ci.triangleIndicesStart += indexCounts[i - 1] / 3;
    				ci.triangleIndicesEnd += indexCounts[i - 1] / 3;
    			}
    			clusterInfo.push_back(ci);
    		}
    		clusterIndexCounts[i] = clusterInfo.size();
    		errorInfo.insert(errorInfo.end(), naniteMesh.errorInfo.begin(), naniteMesh.errorInfo.end());
    	}
    	NaniteAssert(clusterInfo.size() == errorInfo.size(), "clusterInfo.size() should be equal to errorInfo.size()");

    	for (int i = 0; i < naniteObjects.size(); ++i) {
    		auto& naniteObject = naniteObjects[i];
    		auto referenceMeshIndex = std::find(naniteMeshes.begin(), naniteMeshes.end(), *(naniteObject.referenceMesh)) - naniteMeshes.begin();
    		sceneIndicesCount += indexCounts[referenceMeshIndex];
    		maxClusterNum += clusterIndexCounts[referenceMeshIndex];
    	}
    }
    
	void NaniteScene::createNaniteSceneInfo(VulkanExampleBase& link)
    {
    	createVertexIndexBuffer(link);
    	createClusterInfos(link);
    	createBVHNodeInfos(link);
    	for (size_t i = 0; i < depthCounts.size(); i++)
    	{
    		std::cout << "Depth " << i << " has " << depthCounts[i] << " nodes." << std::endl;
    		std::cout << "Depth " << i << " has " << depthLeafCounts[i] << " leaf nodes." << std::endl;
    	}
    	std::cout << "Among each level, largest node count is: " << maxDepthCounts << std::endl;
    	std::cout << "Total cluster count within current scene: " << maxClusterNum << std::endl;
    }
    
    void NaniteScene::createBVHNodeInfos(VulkanExampleBase& link)
	{
	    for (size_t i = 0; i < naniteMeshes.size(); i++)
	    {
	        auto & naniteMesh = naniteMeshes[i];
	        for (size_t j = 0; j < naniteMesh.sortedClusterIndices.size(); j++)
	        {
	            sortedClusterIndices.push_back(naniteMesh.sortedClusterIndices[j] + clusterIndexOffsets[i]);
	        }
	    }
	    virtualRootNode = std::make_shared<NaniteBVHNode>();
	    virtualRootNode->nodeStatus = VIRTUAL_NODE;
	    for (int i = 0; i < naniteObjects.size(); ++i) {
	        auto& naniteObject = naniteObjects[i];
	        naniteObject.reconstructBVH();
	        auto referenceMeshIndex = std::find(naniteMeshes.begin(), naniteMeshes.end(), *(naniteObject.referenceMesh)) - naniteMeshes.begin();
	        naniteObject.rootNode->objectIdx = i;
	        naniteObject.rootNode->meshIdx = referenceMeshIndex;
	        virtualRootNode->children.push_back(naniteObject.rootNode);
	    }

	    // 仅打包非虚拟节点，存储遍历所需的AABB、父节点误差、子节点索引(ivec4)等信�?
	    std::vector<std::shared_ptr<NaniteBVHNode>> flattenedNonVirtualNodes;

	    std::queue<std::shared_ptr<NaniteBVHNode>> nodeQueue;
	    nodeQueue.push(virtualRootNode);

	    uint32_t index = 0;
	    // 两趟BFS遍历：第一趟收集非虚拟节点，第二趟将节点转换为 NodeInfo
	    while (!nodeQueue.empty())
	    {
	        auto currNode = nodeQueue.front();
	        NaniteAssert(currNode->nodeStatus != NaniteBVHNodeStatus::INVALID, "Invalid node!");
	        nodeQueue.pop();
	        if (currNode->nodeStatus != VIRTUAL_NODE) // 将所有非虚拟节点加入数组
	        {
	            flattenedNonVirtualNodes.push_back(currNode);
	            currNode->index = flattenedNonVirtualNodes.size() - 1;
	        }
	        for (auto child: currNode->children)
	        {
	            if (currNode != virtualRootNode) {
	                child->objectIdx = currNode->objectIdx;
	                child->meshIdx = currNode->meshIdx;
	            }
	            nodeQueue.push(child);
	        }
	    }
	    
	    bvhNodeInfos.resize(flattenedNonVirtualNodes.size());

	    for (size_t i = 0; i < flattenedNonVirtualNodes.size(); i++)
	    {
	        auto& currNode = flattenedNonVirtualNodes[i];
	        auto & nodeInfo = bvhNodeInfos[i];
	        nodeInfo.pMinWorld = currNode->pMin;
	        nodeInfo.pMaxWorld = currNode->pMax;
	        nodeInfo.objectId = currNode->objectIdx;
	        nodeInfo.errorWorld.x = currNode->normalizedlodError;
	        nodeInfo.errorWorld.y = currNode->parentNormalizedError;
	        nodeInfo.errorRP = currNode->parentBoundingSphere;
	        nodeInfo.clusterIntervals.x = currNode->start + clusterIndexOffsets[currNode->meshIdx];
	        nodeInfo.clusterIntervals.y = currNode->end + clusterIndexOffsets[currNode->meshIdx];

	        NaniteAssert(currNode->children.size() <= 4, "Invalid node!");
	        for (size_t j = 0; j < currNode->children.size(); ++j)
	        {
	            auto child = currNode->children[j];
	            nodeInfo.childrenNodeIndices[j] = child->index;
	        }

	        NaniteAssert(currNode->depth <= depthCounts.size(), "`depth` should never be over depthCounts.size()");
	        if (currNode->depth == depthCounts.size())
	        {
	            depthCounts.push_back(1);
	        }
	        else
	        {
	            depthCounts[currNode->depth] += 1;
	        }
	        if (currNode->nodeStatus == LEAF)
	        {	
	            NaniteAssert(currNode->clusterIndices.size() <= CLUSTER_GROUP_MAX_SIZE, "this leaf node stores too many cluster indices");
	            int validClusterIndicesSize = 0;
	            bool isValid = true;
	            // 最终验证：确保所�?cluster 索引是连续分配的
	            for (size_t i = 0; i < CLUSTER_GROUP_MAX_SIZE; i++)
	            {
	                if (currNode->clusterIndices[i] >= 0) {
	                    NaniteAssert(isValid, "Invalid cluster indices");
						validClusterIndicesSize += nodeInfo.clusterIntervals[1] - nodeInfo.clusterIntervals[0];
					}
	                else {
	                    isValid = false;
	                }
	            }
	            if (currNode->depth >= depthLeafCounts.size()) {
	                depthLeafCounts.resize(currNode->depth + 1);
	                depthLeafCounts[currNode->depth] = validClusterIndicesSize;
	            }
	            else {
	                depthLeafCounts[currNode->depth] += validClusterIndicesSize;
	            }
	        }
	    }

	    maxDepthCounts = std::max_element(depthCounts.begin(), depthCounts.end())[0];

	    initNodeInfoIndices.resize(depthCounts[0] + 1); // 根据深度0的节点数初始化节点信息索�?
	    initNodeInfoIndices[0] = depthCounts[0];
	    for (size_t i = 1; i <= depthCounts[0]; i++)
	    {
	        initNodeInfoIndices[i] = i-1;
	    }
	}

}
