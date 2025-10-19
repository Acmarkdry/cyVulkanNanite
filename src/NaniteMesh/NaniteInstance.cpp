#include "NaniteInstance.h"

#include "NaniteLodMesh.h"
#include "NaniteMesh.h"
#include "../utils.h"

#include <numeric>
#include <optional>

#include "NaniteBVH.h"

namespace Nanite
{
	void NaniteInstance::reconstructBVH()
	{
        // 从展平的BVH重建树结构。
        // 对于N个实例，由于有N个变换矩阵，需要构建N棵BVH树。
        // 然后在NaniteScene中创建虚拟根节点连接所有实例的虚拟节点，再展平整棵BVH。

        std::vector<std::shared_ptr<NaniteBVHNode>> flattenedBVHNodes(referenceMesh->flattenedBVHNodeInfos.size(), nullptr);
        
        std::vector<uint32_t> clusterIndexOffset; // 不同LOD层级导致的 cluster 索引偏移
        clusterIndexOffset.resize(referenceMesh->meshes.size(), 0);
        for (size_t i = 1; i < clusterIndexOffset.size(); i++)
        {
            clusterIndexOffset[i] = clusterIndexOffset[i - 1] + referenceMesh->meshes[i-1].clusterNum;
        }
        std::unordered_set<uint32_t> clusterIndexSet;
        uint32_t totalClusterNum = clusterIndexOffset.back() + referenceMesh->meshes.back().clusterNum;
        for (size_t i = 0; i < totalClusterNum; i++)
        {
            clusterIndexSet.insert(i);
        }
        for (size_t i = 0; i < referenceMesh->flattenedBVHNodeInfos.size(); i++)
        {
            auto & nodeInfo = referenceMesh->flattenedBVHNodeInfos[i];
            auto& currNode = flattenedBVHNodes[i];
            currNode = std::make_shared<NaniteBVHNode>();
            currNode->depth = nodeInfo.depth;
            currNode->parentNormalizedError = nodeInfo.parentNormalizedError;
            currNode->normalizedlodError = nodeInfo.normalizedlodError;
            glm::vec3 parentCenter(nodeInfo.parentBoundingSphere);
            parentCenter = glm::vec3(rootTransform * glm::vec4(parentCenter, 1.0));
            currNode->parentBoundingSphere = glm::vec4(parentCenter, nodeInfo.parentBoundingSphere.w);
            // 应用变换矩阵
            currNode->pMin = glm::vec3(rootTransform * glm::vec4(nodeInfo.pMin, 1.0f));
            currNode->pMax = glm::vec3(rootTransform * glm::vec4(nodeInfo.pMax, 1.0f));
            currNode->nodeStatus = nodeInfo.nodeStatus;
            currNode->index = nodeInfo.index;
            currNode->lodLevel = nodeInfo.lodLevel;
            currNode->start = nodeInfo.start;
            currNode->end = nodeInfo.end;
            NaniteAssert(currNode->nodeStatus == VIRTUAL_NODE || currNode->lodLevel >= 0, "lodLevel of any non-root node is negative!");
            NaniteAssert(currNode->nodeStatus == LEAF || currNode->clusterIndices[0] == -1, "non-leaf node also has a valid cluster index!");
            for (size_t j = 0; j < currNode->clusterIndices.size(); j++)
            {
                if (nodeInfo.clusterIndices[j] >= 0)
                {
                    currNode->clusterIndices[j] = nodeInfo.clusterIndices[j] + clusterIndexOffset[currNode->lodLevel];
                    NaniteAssert(clusterIndexSet.find(currNode->clusterIndices[j]) != clusterIndexSet.end(), "Duplicated cluster index!");
                    clusterIndexSet.erase(currNode->clusterIndices[j]);
                }
            }
            NaniteAssert(currNode->nodeStatus != INVALID, "Invalid nodes!");
        }
        NaniteAssert(clusterIndexSet.empty(), "Unused cluster index!");

        for (size_t i = 0; i < referenceMesh->flattenedBVHNodeInfos.size(); i++)
        {
            auto & nodeInfo = referenceMesh->flattenedBVHNodeInfos[i];
            for (auto childIndex: nodeInfo.children)
            {
                flattenedBVHNodes[i]->children.push_back(flattenedBVHNodes[childIndex]);
            }
        }

        rootNode = flattenedBVHNodes[0];
    }
	

	glm::vec3 NaniteInstance::transformPoint(const glm::vec3& point) const
	{
		return glm::vec3(rootTransform * glm::vec4(point, 1.0f));
	}

	glm::vec3 NaniteInstance::openMeshPointToGlm(const NaniteTriMesh::Point& point) const
	{
		return glm::vec3(point[0], point[1], point[2]);
	}

	float NaniteInstance::calculateWorldRadius(float localRadius) const
	{
		return glm::length(rootTransform * glm::vec4(localRadius, 0.0f, 0.0f, 0.0f));
	}

	void NaniteInstance::processFaceAABB(const NaniteLodMesh& lodMesh, size_t currClusterNum)
	{
		const auto& mesh = lodMesh.mesh;

		for (auto face_it = mesh.faces_begin(); face_it != mesh.faces_end(); ++face_it)
		{
			const auto fh = *face_it;
			const auto clusterIdx = lodMesh.triangleClusterIndex[fh.idx()] + currClusterNum;
			auto& clusterI = clusterInfo[clusterIdx];

			// 获取三角形顶点
			auto fv_it = mesh.cfv_iter(fh);
			const auto p0 = transformPoint(openMeshPointToGlm(mesh.point(*fv_it++)));
			const auto p1 = transformPoint(openMeshPointToGlm(mesh.point(*fv_it++)));
			const auto p2 = transformPoint(openMeshPointToGlm(mesh.point(*fv_it)));

			glm::vec3 pMinWorld, pMaxWorld;
			getTriangleAABB(p0, p1, p2, pMinWorld, pMaxWorld);
			clusterI.mergeAABB(pMinWorld, pMaxWorld);
		}
	}

	void NaniteInstance::processClusterIndices(const NaniteLodMesh& lodMesh, size_t currClusterNum, size_t currTriangleNum)
	{
		const auto& sortedIndices = lodMesh.triangleIndicesSortedByClusterIdx;
		const auto& clusterIndex = lodMesh.triangleClusterIndex;

		std::optional<uint32_t> prevClusterIdx;

		for (size_t j = 0; j < sortedIndices.size(); ++j)
		{
			const auto currTriangleIndex = sortedIndices[j];
			const auto currClusterIdx = clusterIndex[currTriangleIndex];

			if (!prevClusterIdx.has_value() || currClusterIdx != prevClusterIdx.value())
			{
				if (prevClusterIdx.has_value())
				{
					clusterInfo[prevClusterIdx.value() + currClusterNum].triangleIndicesEnd = j + currTriangleNum;
				}
				clusterInfo[currClusterIdx + currClusterNum].triangleIndicesStart = j + currTriangleNum;
				prevClusterIdx = currClusterIdx;
			}
		}

		// 处理最后一个cluster
		if (prevClusterIdx.has_value())
		{
			clusterInfo[prevClusterIdx.value() + currClusterNum].triangleIndicesEnd = sortedIndices.size() + currTriangleNum;
		}
	}

	void NaniteInstance::processClusterErrors(const NaniteLodMesh& lodMesh, size_t meshIndex, size_t currClusterNum)
	{
		const auto& meshes = referenceMesh->meshes;
		const bool isLastLevel = (meshIndex == meshes.size() - 1);

		for (size_t j = 0; j < lodMesh.clusters.size(); ++j)
		{
			const auto& cluster = lodMesh.clusters[j];
			const auto globalClusterIdx = j + currClusterNum;

			// 计算error
			const float parentError = isLastLevel ? 1e5f : cluster.parentNormalizedError;
			errorInfo[globalClusterIdx].errorWorld = glm::vec2(cluster.normalizedlodError, parentError);

			// 计算世界空间包围球
			const auto worldCenter = transformPoint(cluster.boundingSphereCenter);
			const float worldRadius = calculateWorldRadius(cluster.boundingSphereRadius);

			NaniteAssert(cluster.triangleIndices.size() <= CLUSTER_MAX_SIZE, "cluster.triangleIndices.size() is over threshold");
			NaniteAssert(cluster.boundingSphereRadius > 0 || cluster.triangleIndices.empty(), "boundingSphereRadius <= 0");
			NaniteAssert(worldRadius > 0 || cluster.triangleIndices.empty(), "worldRadius <= 0");

			errorInfo[globalClusterIdx].centerR = glm::vec4(worldCenter, worldRadius);

			// 计算父级包围球
			float parentBoundingRadius = 0.0f;
			glm::vec3 parentCenter{0.0f};

			if (isLastLevel)
			{
				parentBoundingRadius = worldRadius * 1.5f;
				parentCenter = cluster.boundingSphereCenter;
			}
			else if (!cluster.parentClusterIndices.empty())
			{
				const auto parentIdx = cluster.parentClusterIndices.front();
				const auto& parentCluster = meshes[meshIndex + 1].clusters[parentIdx];
				parentBoundingRadius = parentCluster.boundingSphereRadius;
				parentCenter = parentCluster.boundingSphereCenter;
			}

			const auto parentWorldCenter = transformPoint(parentCenter);
			errorInfo[globalClusterIdx].centerRP = glm::vec4(parentWorldCenter, parentBoundingRadius);
		}
	}

	void NaniteInstance::buildClusterInfo()
	{
		const auto& meshes = referenceMesh->meshes;

		// 计算总cluster数量
		const size_t totalClusterNum = std::accumulate(meshes.begin(), meshes.end(), size_t{0}, [](size_t sum, const NaniteLodMesh& mesh) { return sum + mesh.clusterNum; });

		clusterInfo.resize(totalClusterNum);
		errorInfo.resize(totalClusterNum);

		size_t currClusterNum = 0;
		size_t currTriangleNum = 0;

		for (size_t i = 0; i < meshes.size(); ++i)
		{
			const auto& lodMesh = meshes[i];

			processFaceAABB(lodMesh, currClusterNum);
			processClusterIndices(lodMesh, currClusterNum, currTriangleNum);
			processClusterErrors(lodMesh, i, currClusterNum);

			currClusterNum += lodMesh.clusterNum;
			currTriangleNum += lodMesh.triangleIndicesSortedByClusterIdx.size();
		}
	}

	void NaniteInstance::initBufferForNaniteLODs()
	{
		const auto& meshes = referenceMesh->meshes;

		// 计算总顶点和索引数量
		size_t totalNumVertices = 0;
		size_t totalNumIndices = 0;

		for (const auto& lodMesh : meshes)
		{
			assert(!lodMesh.uniqueVertexBuffer.empty());
			assert(!lodMesh.triangleVertexIndicesSortedByClusterIdx.empty());

			totalNumVertices += lodMesh.uniqueVertexBuffer.size();
			totalNumIndices += lodMesh.triangleVertexIndicesSortedByClusterIdx.size();
		}

		vertexBuffer.clear();
		indexBuffer.clear();
		vertexBuffer.reserve(totalNumVertices);
		indexBuffer.reserve(totalNumIndices);

		size_t currVertSize = 0;
		for (const auto& lodMesh : meshes)
		{
			// 批量插入顶点
			vertexBuffer.insert(vertexBuffer.end(), lodMesh.uniqueVertexBuffer.begin(), lodMesh.uniqueVertexBuffer.end());

			// 添加偏移后的索引
			for (const auto index : lodMesh.triangleVertexIndicesSortedByClusterIdx)
			{
				indexBuffer.emplace_back(static_cast<uint32_t>(index + currVertSize));
			}

			currVertSize += lodMesh.uniqueVertexBuffer.size();
		}
	}
}
