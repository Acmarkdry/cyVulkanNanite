#include "NaniteMesh.h"

#include <queue>

#include "BVH.h"
#include "NaniteLodMesh.h"
#include "../utils.h"

namespace Nanite
{
	void NaniteMesh::loadvkglTFModel(const vkglTF::Model& model)
	{
		vkglTFModel = &model;
		for (auto& node : vkglTFModel->linearNodes)
		{
			if (node->mesh)
			{
				vkglTFMesh = node->mesh;
				modelMatrix = node->getMatrix();
				break;
			}
		}
	}

	void NaniteMesh::generateNaniteInfo()
	{
		NaniteTriMesh naniteTriMesh;
		vkglTFMeshToOpenMesh(naniteTriMesh, *vkglTFMesh);

		int clusterGroupNum = -1;
		int target = 1; // 先只进行一次
		int currFaceNum = -1;
		naniteTriMesh.add_property(clusterGroupIndexPropHandle);

		// lod
		do
		{
			NaniteLodMesh naniteLodMesh;
			naniteLodMesh.mesh = naniteTriMesh;
			// cluster
			naniteLodMesh.buildTriangleGraph();
			naniteLodMesh.generateCluster();
			
			// cluster graph
			naniteLodMesh.buildClusterGraph();
			naniteLodMesh.colorClusterGraph();
			naniteLodMesh.generateClusterGroup();
			
			// TODO mesh简化
			
			meshes.push_back(naniteLodMesh);
		}
		while (--target);
		
		// TODO bvh tree
		clusterIndexOffset.resize(meshes.size(), 0);
		for (size_t i = 0; i < meshes.size(); i++)
		{
			if (i != 0)
			{
				clusterIndexOffset[i] = clusterIndexOffset[i - 1] + meshes[i - 1].clusterNum;
			}
			meshes[i].createBVH();
		}
		flattenBVH();
	}

	void NaniteMesh::vkglTFPrimitiveToOpenMesh(NaniteTriMesh& naniteTriMesh, const vkglTF::Primitive& prim)
	{
		int vertStart = prim.firstVertex;
		int vertEnd = prim.firstVertex + prim.vertexCount;
		std::vector<NaniteTriMesh::VertexHandle> vhandles;
		for (int i = vertStart; i != vertEnd; i++)
		{
			auto& vert = vkglTFModel->vertexBuffer[i];
			auto vhandle = naniteTriMesh.add_vertex(NaniteTriMesh::Point(vert.pos.x, vert.pos.y, vert.pos.z));
			naniteTriMesh.set_normal(vhandle, NaniteTriMesh::Normal(vert.normal.x, vert.normal.y, vert.normal.z));
			naniteTriMesh.set_texcoord2D(vhandle, NaniteTriMesh::TexCoord2D(vert.uv.x, vert.uv.y));
			vhandles.emplace_back(vhandle);
		}
		int indStart = prim.firstIndex;
		int indEnd = prim.firstIndex + prim.indexCount;
		for (int i = indStart; i != indEnd; i += 3)
		{
			int i0 = vkglTFModel->indexBuffer[i] - vertStart, i1 = vkglTFModel->indexBuffer[i + 1] - vertStart, i2 =
				    vkglTFModel->indexBuffer[i + 2] - vertStart;
			std::vector<NaniteTriMesh::VertexHandle> face_vhandles;
			face_vhandles.clear();
			face_vhandles.emplace_back(vhandles[i0]);
			face_vhandles.emplace_back(vhandles[i1]);
			face_vhandles.emplace_back(vhandles[i2]);
			naniteTriMesh.add_face(face_vhandles);
		}
	}


	void NaniteMesh::vkglTFMeshToOpenMesh(NaniteTriMesh& naniteTriMesh, const vkglTF::Mesh& mesh)
	{
		for (auto& prim : mesh.primitives)
		{
			vkglTFPrimitiveToOpenMesh(naniteTriMesh, *prim);
			// 为网格的face edge vertex分配额外内存，用来存储状态属性
			naniteTriMesh.request_face_status();
			naniteTriMesh.request_edge_status();
			naniteTriMesh.request_vertex_status();
		}
	}

	void NaniteMesh::flattenBVH()
	{
	}

	void NaniteMesh::serialize(const std::string& filepath)
	{
	}

	void NaniteMesh::deserialize(const std::string& filepath)
	{
	}

	void NaniteMesh::initNaniteInfo(const std::string& filepath, bool useCache)
	{
		bool hasCache = false;
		bool hasInitialized = false;
		std::string cachePath;
		if (filepath.find_last_of(".") != std::string::npos)
		{
			cachePath = filepath.substr(0, filepath.find_last_of('.')) + "_naniteCache\\";
		}
		else
		{
			NaniteAssert(false, "Invalid file path, no ext");
		}

		if (useCache)
		{
			std::ifstream inputFile(cachePath + "nanite_info.json");
			if (inputFile.is_open())
			{
				deserialize(cachePath);
				hasInitialized = true;
			}
			else
			{
				std::cerr << "No cache, need to initialize from now" << std::endl;
			}
		}

		if (!hasInitialized)
		{
			std::cerr << "Start building..." << std::endl;
			// 这里开始生成cluster
			generateNaniteInfo();
			serialize(cachePath);
			std::cout << cachePath << "nanite_info.json" << " generated" << std::endl;
		}
	}

	bool NaniteMesh::operator==(const NaniteMesh& other) const
	{
		if (meshes.size() != other.meshes.size()) return false;
		for (int i = 0; i < meshes.size(); i++)
		{
			if (meshes[i].mesh.n_vertices() != other.meshes[i].mesh.n_vertices()) return false;
			if (meshes[i].mesh.n_faces() != other.meshes[i].mesh.n_faces()) return false;
		}
		return true;
	}

	void NaniteMesh::buildClusterInfo()
	{
		NaniteAssert(false, "buildClusterInfo not implemented");
	}
}
