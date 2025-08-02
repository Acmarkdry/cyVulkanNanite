#include "NaniteInstance.h"

#include "NaniteLodMesh.h"
#include "NaniteMesh.h"

namespace Nanite
{
	void NaniteInstance::buildClusterInfo()
	{
		clusterInfos.resize(referenceMesh->clusterNum);
		auto& mesh = referenceMesh->mesh;
		
		// 构建cluster的包围盒
		for (NaniteTriMesh::FaceIter faceIt = mesh.faces_begin(); faceIt != mesh.faces_end(); ++faceIt)
		{
			NaniteTriMesh::FaceHandle faceHandle = *faceIt;
			auto clusterIdx = referenceMesh->triangleClusterIndex[faceHandle.idx()];
			auto &clusterInstance = clusterInfos[clusterIdx];
			
			glm::vec3 pMinWorld, pMaxWorld, p0, p1, p2;
			NaniteTriMesh::FaceVertexIter faceVerter = mesh.fv_iter(faceHandle);
			
			auto point0 = mesh.point(*faceVerter);
			++faceVerter;
			auto point1 = mesh.point(*faceVerter);
			++faceVerter;
			auto point2 = mesh.point(*faceVerter);
			++faceVerter;
			
			auto convertVec3fToGlm = [](const OpenMesh::Vec3f& input, glm::vec3 &output) {
				output[0] = input[0];
				output[1] = input[1];
				output[2] = input[2];
			};
			
			convertVec3fToGlm(point0, p0);
			convertVec3fToGlm(point1, p1);
			convertVec3fToGlm(point2, p2);
			
			p0 = glm::vec3(transform*glm::vec4(p0, 1.0));
			p1 = glm::vec3(transform*glm::vec4(p1, 1.0));
			p2 = glm::vec3(transform*glm::vec4(p2, 1.0));
			
			pMinWorld = glm::min(p0, glm::min(p1, p2));
			pMaxWorld = glm::max(p0, glm::max(p1, p2));
			
			clusterInstance.mergeAABB(pMinWorld, pMaxWorld);
		}
		
		// 计算cluster的索引范围
		int32_t currClusterIdx = -1;
		for (size_t clusterIdx = 0; clusterIdx < referenceMesh->triangleIndicesSortedByClusterIdx.size(); ++clusterIdx)
		{	
			// cluster->triangleIdx->cluster
			uint32_t currTriangleIndex = referenceMesh->triangleIndicesSortedByClusterIdx[clusterIdx];
			if (referenceMesh->triangleClusterIndex[currTriangleIndex] != currClusterIdx)
			{
				if (currClusterIdx != -1)
					clusterInfos[currClusterIdx].triangleIndicesEnd = clusterIdx;
				currClusterIdx = referenceMesh->triangleClusterIndex[currTriangleIndex];
				clusterInfos[currClusterIdx].triangleIndicesEnd = clusterIdx;
			}
		}
		
		clusterInfos[currClusterIdx].triangleIndicesEnd = referenceMesh->triangleIndicesSortedByClusterIdx.size();
	
	}
}
