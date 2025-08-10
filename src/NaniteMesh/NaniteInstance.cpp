#include "NaniteInstance.h"

#include "NaniteLodMesh.h"
#include "NaniteMesh.h"
#include "../vksTools.h"
#include "../utils.h"

namespace Nanite
{
	void NaniteInstance::createBuffersForNaniteLod(VulkanExampleBase &link)
	{
        std::vector<vkglTF::Vertex> vertexBuffer;
        std::vector<uint32_t> indexBuffer;
        size_t totalNumVertices = 0;
        size_t totalNumIndices = 0;
        for (int i = 0; i < referenceMesh->meshes.size(); i++)
        {
            NaniteAssert(referenceMesh->meshes[i].uniqueVertexBuffer.size() > 0, "unique vertex buffer size is zero");
            totalNumVertices += referenceMesh->meshes[i].uniqueVertexBuffer.size();
            NaniteAssert(referenceMesh->meshes[i].triangleVertexIndicesSortedByClusterIdx.size() > 0, "triangle vertex indices size is zero");
            totalNumIndices += referenceMesh->meshes[i].triangleVertexIndicesSortedByClusterIdx.size();
        }
        vertexBuffer.reserve(totalNumVertices);
        indexBuffer.reserve(totalNumIndices);
        size_t currVertSize = 0;
        for (int i = 0; i < referenceMesh->meshes.size(); i++)
        {
            for (auto& vert : referenceMesh->meshes[i].uniqueVertexBuffer)
            {
                vertexBuffer.emplace_back(vert);
            }
            for (auto& index : referenceMesh->meshes[i].triangleVertexIndicesSortedByClusterIdx)
            {
                indexBuffer.emplace_back(index + currVertSize);
            }
            currVertSize += referenceMesh->meshes[i].uniqueVertexBuffer.size();
        }

        size_t vertexBufferSize = vertexBuffer.size() * sizeof(vkglTF::Vertex);
        vertices.size = static_cast<uint32_t>(vertexBuffer.size());
		
		vks::vksTools::createStagingBuffer(link, 0, vertexBufferSize, vertexBuffer.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices);
		
        size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
        indices.size = static_cast<uint32_t>(indexBuffer.size());
		vks::vksTools::createStagingBuffer(link, 0, indexBufferSize, indexBuffer.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,  indices);
	}

	void NaniteInstance::buildClusterInfo()
	{
        // Init Clusters
        size_t totalClusterNum = 0;
        for (int i = 0; i < referenceMesh->meshes.size(); i++)
        {
            totalClusterNum += referenceMesh->meshes[i].clusterNum;
#ifdef DEBUG_LOD_START
            break;
#endif // DEBUG_LOD_START
        }
        clusterInfo.resize(totalClusterNum);
        errorInfo.resize(totalClusterNum);
        size_t currClusterNum = 0, currTriangleNum = 0;
        for (int i = 0; i < referenceMesh->meshes.size(); i++)
        {
            auto& mesh = referenceMesh->meshes[i].mesh;
            for (NaniteTriMesh::FaceIter face_it = mesh.faces_begin(); face_it != mesh.faces_end(); ++face_it) {
                NaniteTriMesh::FaceHandle fh = *face_it;
                auto clusterIdx = referenceMesh->meshes[i].triangleClusterIndex[fh.idx()] + currClusterNum;
                auto& clusterI = clusterInfo[clusterIdx];

                glm::vec3 pMinWorld, pMaxWorld;
                glm::vec3 p0, p1, p2;
                NaniteTriMesh::FaceVertexIter fv_it = mesh.fv_iter(fh);

                // Get the positions of the three vertices
                auto point0 = mesh.point(*fv_it);
                ++fv_it;
                auto point1 = mesh.point(*fv_it);
                ++fv_it;
                auto point2 = mesh.point(*fv_it);

                p0[0] = point0[0];
                p0[1] = point0[1];
                p0[2] = point0[2];

                p1[0] = point1[0];
                p1[1] = point1[1];
                p1[2] = point1[2];

                p2[0] = point2[0];
                p2[1] = point2[1];
                p2[2] = point2[2];

                p0 = glm::vec3(rootTransform * glm::vec4(p0, 1.0f));
                p1 = glm::vec3(rootTransform * glm::vec4(p1, 1.0f));
                p2 = glm::vec3(rootTransform * glm::vec4(p2, 1.0f));

                getTriangleAABB(p0, p1, p2, pMinWorld, pMaxWorld);

                clusterI.mergeAABB(pMinWorld, pMaxWorld);
            }


            uint32_t currClusterIdx = -1;
            for (size_t j = 0; j < referenceMesh->meshes[i].triangleIndicesSortedByClusterIdx.size(); j++)
            {
                auto currTriangleIndex = referenceMesh->meshes[i].triangleIndicesSortedByClusterIdx[j];
                if (referenceMesh->meshes[i].triangleClusterIndex[currTriangleIndex] != currClusterIdx)
                {
                    if (currClusterIdx != -1) {
                        //std::cout << "Cluster " << currClusterIdx << " end at " << j << std::endl;
                        clusterInfo[currClusterIdx + currClusterNum].triangleIndicesEnd = j + currTriangleNum;
                    }
                    currClusterIdx = referenceMesh->meshes[i].triangleClusterIndex[currTriangleIndex];
                    clusterInfo[currClusterIdx + currClusterNum].triangleIndicesStart = j + currTriangleNum;
                    //std::cout << "Cluster " << currClusterIdx << " start at " << j << std::endl;
                }
            }
            clusterInfo[currClusterIdx + currClusterNum].triangleIndicesEnd = referenceMesh->meshes[i].triangleIndicesSortedByClusterIdx.size() + currTriangleNum;

            for (size_t j = 0; j < referenceMesh->meshes[i].clusters.size(); j++)
            {
                auto& cluster = referenceMesh->meshes[i].clusters[j];
                float parentError = i == referenceMesh->meshes.size() - 1 ? 1e5 : cluster.parentNormalizedError;
                errorInfo[j + currClusterNum].errorWorld = glm::vec2(cluster.lodError, parentError);
                glm::vec3 worldCenter = glm::vec3(rootTransform * glm::vec4(cluster.boundingSphereCenter, 1.0));
                //TODO: 任意比例缩放
                float worldRadius = glm::length(rootTransform * glm::vec4(glm::vec3(cluster.boundingSphereRadius,0,0), 0.0));
                errorInfo[j + currClusterNum].centerR = glm::vec4(worldCenter, worldRadius);
                float maxParentBoundingRadius = 0;
                glm::vec3 parentCenter = glm::vec3(0);
                if (i == referenceMesh->meshes.size() - 1)//last level of lod, no parent
                {
                    maxParentBoundingRadius = 1e-6;
                    parentCenter = cluster.boundingSphereCenter;
                }
                else for (size_t k : cluster.parentClusterIndices)//get max parent bounding sphere size
                {
                    maxParentBoundingRadius = std::max(maxParentBoundingRadius, referenceMesh->meshes[i + 1].clusters[k].boundingSphereRadius);
                    parentCenter += referenceMesh->meshes[i + 1].clusters[k].boundingSphereCenter;
                }
                parentCenter /= i == referenceMesh->meshes.size() - 1 ? 1.0 : cluster.parentClusterIndices.size();
                glm::vec3 parentWorldCenter = glm::vec3(rootTransform * glm::vec4(parentCenter, 1.0));
                //TODO: 任意比例缩放
                float parentWorldRadius = glm::length(rootTransform * glm::vec4(glm::vec3(maxParentBoundingRadius, 0, 0), 0.0));
                errorInfo[j + currClusterNum].centerRP = glm::vec4(parentWorldCenter, parentWorldRadius);
            }
            currClusterNum += referenceMesh->meshes[i].clusterNum;
            currTriangleNum += referenceMesh->meshes[i].triangleIndicesSortedByClusterIdx.size();
#ifdef DEBUG_LOD_START
            break;
#endif // DEBUG_LOD_START
        }
	}
}
