#include "NaniteInstance.h"

#include "NaniteLodMesh.h"
#include "NaniteMesh.h"
#include "../utils.h"

#include <algorithm>
#include <numeric>
#include <optional>

namespace Nanite
{
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

			NaniteAssert(cluster.triangleIndices.size() <= CLUSTER_THRESHOLD, "cluster.triangleIndices.size() is over threshold");
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
