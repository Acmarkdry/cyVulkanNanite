#include "NaniteMesh.h"

#include <queue>

#include "BVH.h"
#include "NaniteLodMesh.h"
#include "../utils.h"
#include <filesystem>
#include <json.hpp>
#include <OpenMesh/Core/IO/MeshIO.hh>

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

	void NaniteMesh::vkglTFPrimitiveToOpenMesh(NaniteTriMesh& mymesh, const vkglTF::Primitive& prim)
	{
		int vertStart = prim.firstVertex;
		int vertEnd = prim.firstVertex + prim.vertexCount;
		std::vector<NaniteTriMesh::VertexHandle> vhandles;
		for (int i = vertStart; i != vertEnd; i++)
		{
			auto& vert = vkglTFModel->vertexBuffer[i];
			auto vhandle = mymesh.add_vertex(NaniteTriMesh::Point(vert.pos.x, vert.pos.y, vert.pos.z));
			mymesh.set_normal(vhandle, NaniteTriMesh::Normal(vert.normal.x, vert.normal.y, vert.normal.z));
			mymesh.set_texcoord2D(vhandle, NaniteTriMesh::TexCoord2D(vert.uv.x, vert.uv.y));
			vhandles.emplace_back(vhandle);
		}
		int indStart = prim.firstIndex;
		int indEnd = prim.firstIndex + prim.indexCount;
		for (int i = indStart; i != indEnd; i += 3)
		{
			int i0 = vkglTFModel->indexBuffer[i] - vertStart, i1 = vkglTFModel->indexBuffer[i + 1] - vertStart, i2 = vkglTFModel->indexBuffer[i + 2] - vertStart;
			std::vector<NaniteTriMesh::VertexHandle> face_vhandles;
			face_vhandles.clear();
			face_vhandles.emplace_back(vhandles[i0]);
			face_vhandles.emplace_back(vhandles[i1]);
			face_vhandles.emplace_back(vhandles[i2]);
			mymesh.add_face(face_vhandles);
		}
	}
	
	void NaniteMesh::vkglTFMeshToOpenMesh(NaniteTriMesh& mymesh, const vkglTF::Mesh& mesh)
	{
		for (auto& prim : mesh.primitives)
		{
			vkglTFPrimitiveToOpenMesh(mymesh, *prim);

			mymesh.request_face_status();
			mymesh.request_edge_status();
			mymesh.request_vertex_status();
		}
	}
	
	void NaniteMesh::generateNaniteInfo()
	{
		NaniteTriMesh mymesh;
		vkglTFMeshToOpenMesh(mymesh, *vkglTFMesh);
		int clusterGroupNum = -1;
		int target = 6;
		int currFaceNum = -1;
		
		mymesh.add_property(clusterGroupIndexPropHandle);
		do
		{
			// For each lod mesh
			NaniteLodMesh meshLOD;
			meshLOD.mesh = mymesh;
			meshLOD.lodLevel = lodNums;
			meshLOD.clusterGroupIndexPropHandle = clusterGroupIndexPropHandle;
			if (clusterGroupNum > 0)
			{
				meshLOD.oldClusterGroups.resize(clusterGroupNum);
				meshLOD.assignTriangleClusterGroup(meshes.back());
			}
			else
			{
				meshLOD.buildTriangleGraph();
				meshLOD.generateCluster();
			}

			meshLOD.buildClusterGraph();
			meshLOD.colorClusterGraph();
			meshLOD.generateClusterGroup();
			currFaceNum = meshLOD.mesh.n_faces();
			clusterGroupNum = meshLOD.clusterGroupNum;

			mymesh = meshLOD.mesh;
			if (clusterGroupNum > 1)
			{
				meshLOD.simplifyMesh(mymesh);
			}
			meshes.emplace_back(meshLOD);
			std::cout << "LOD " << lodNums++ << " generated" << std::endl;
		}
		while (--target);
	}

	void NaniteMesh::serialize(const std::string& filepath)
	{
		std::filesystem::path directoryPath(filepath);

		try
		{
			if (std::filesystem::create_directory(directoryPath))
			{
				std::cout << "Directory created successfully." << std::endl;
			}
			else
			{
				std::cout << "Failed to create directory or it already exists. Dir:" << filepath << std::endl;
			}
		}
		catch (const std::filesystem::filesystem_error& e)
		{
			NaniteAssert(false, "Error creating directory");
		}

		for (size_t i = 0; i < meshes.size(); i++)
		{
			auto& mesh = meshes[i];
			std::string output_filename = std::string(filepath) + "LOD_" + std::to_string(i) + ".obj";
			// Export the mesh to the specified file
			if (!OpenMesh::IO::write_mesh(mesh.mesh, output_filename, OpenMesh::IO::Options::VertexNormal | OpenMesh::IO::Options::VertexTexCoord))
			{
				std::cerr << "Error exporting mesh to " << output_filename << std::endl;
			}
		}

		nlohmann::json result;
		for (size_t i = 0; i < meshes.size(); i++)
		{
			result["mesh"][i] = meshes[i].toJson();
		}
		result[cache_time_key] = std::time(nullptr);
		result["lodNums"] = lodNums;

		// Save the JSON data to a file
		std::ofstream file(std::string(filepath) + "nanite_info.json");
		if (file.is_open())
		{
			file << result.dump(2); // Pretty-print with an indentation of 2 spaces
			file.close();
		}
		else
		{
			NaniteAssert(false, "Error opening file for serialization");
		}
	}

	void NaniteMesh::deserialize(const std::string& filepath)
	{
		std::ifstream inputFile(std::string(filepath) + "nanite_info.json");

		NaniteAssert(inputFile.is_open(), "Error opening file for deserialization");
		nlohmann::json loadedJson;
		inputFile >> loadedJson;

		lodNums = loadedJson["lodNums"].get<uint32_t>();
		meshes.resize(lodNums);
		for (int i = 0; i < lodNums; ++i)
		{
			auto& meshLOD = meshes[i];
			meshLOD.fromJson(loadedJson["mesh"][i]);

			float percentage = static_cast<float>(i + 1) / lodNums * 100.0;
			std::cout << "\r";
			std::cout << "[Loading] Mesh Info: " << std::fixed << std::setw(6) << std::setprecision(2) << percentage << "%";
			std::cout.flush();
		}
		std::cout << std::endl;

		for (size_t i = 0; i < lodNums; i++)
		{
			std::string output_filename = std::string(filepath) + "LOD_" + std::to_string(i) + ".obj";
			meshes[i].mesh.request_vertex_normals();
			meshes[i].mesh.request_vertex_texcoords2D();
			OpenMesh::IO::Options opt = OpenMesh::IO::Options::VertexNormal | OpenMesh::IO::Options::VertexTexCoord;
			if (!OpenMesh::IO::read_mesh(meshes[i].mesh, output_filename, opt))
			{
				NaniteAssert(false, "failed to load mesh");
			}
			NaniteAssert(meshes[i].mesh.has_vertex_normals(), "mesh has no normals");
			meshes[i].lodLevel = i;

			std::cout << "\r";
			float percentage = static_cast<float>(i + 1) / lodNums * 100.0;
			std::cout << "[Loading] Mesh LOD: " << std::fixed << std::setw(6) << std::setprecision(2) << percentage << "%";
			std::cout.flush();
		}
		std::cout << std::endl;
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
			// TODO: Check cache time to see if cache needs to be rebuilt
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
			generateNaniteInfo();
			serialize(cachePath);
			std::cout << cachePath << "nanite_info.json" << " generated" << std::endl;
			//checkDeserializationResult(cachePath);
		}
	}

	void NaniteMesh::checkDeserializationResult(const std::string& filepath)
	{
		std::ifstream inputFile(std::string(filepath) + "nanite_info.json");

		NaniteAssert(inputFile.is_open(), "Error opening file for deserialization");
		nlohmann::json loadedJson;
		inputFile >> loadedJson;

		lodNums = loadedJson["lodNums"].get<uint32_t>();
		debugMeshes.resize(lodNums);
		for (int i = 0; i < lodNums; ++i)
		{
			auto& meshLOD = debugMeshes[i];
			meshLOD.fromJson(loadedJson["mesh"][i]);
		}

		for (size_t i = 0; i < lodNums; i++)
		{
			std::string output_filename = std::string(filepath) + "LOD_" + std::to_string(i) + ".obj";
			debugMeshes[i].mesh.request_vertex_normals();
			OpenMesh::IO::Options opt = OpenMesh::IO::Options::VertexNormal;
			if (!OpenMesh::IO::read_mesh(debugMeshes[i].mesh, output_filename, opt))
			{
				NaniteAssert(false, "failed to load mesh");
			}
			NaniteAssert(debugMeshes[i].mesh.has_vertex_normals(), "mesh has no normals");
			debugMeshes[i].lodLevel = i;
		}

		for (size_t i = 0; i < lodNums; i++)
		{
			auto& mesh = meshes[i];
			auto& debugMesh = debugMeshes[i];
			TEST(mesh.clusters.size() == debugMesh.clusters.size(), "cluster size match");
			for (size_t clusterIdx = 0; clusterIdx < mesh.clusters.size(); clusterIdx++)
			{
				const auto& cluster = mesh.clusters[clusterIdx];
				const auto& debugCluster = debugMesh.clusters[clusterIdx];
				TEST(cluster.parentNormalizedError == debugCluster.parentNormalizedError, "parentNormalizedError match");
				TEST(cluster.lodError == debugCluster.lodError, "lodError match");
				TEST(cluster.boundingSphereCenter == debugCluster.boundingSphereCenter, "boundingSphereCenter match");
				TEST(cluster.boundingSphereRadius == debugCluster.boundingSphereRadius, "boundingSphereRadius match");
			}
			TEST(mesh.mesh.n_faces() == debugMesh.mesh.n_faces(), "face size match");
			TEST(mesh.mesh.n_vertices() == debugMesh.mesh.n_vertices(), "vertex size match");
			for (const auto& vhandle : mesh.mesh.vertices())
			{
				auto debugVhandle = debugMesh.mesh.vertex_handle(vhandle.idx());
				//std::cout << "mesh normal: " << mesh.mesh.normal(vhandle)[0] << " " << mesh.mesh.normal(vhandle)[1] << " " << mesh.mesh.normal(vhandle)[2] << std::endl;
				//std::cout << "debug normal: " << debugMesh.mesh.normal(debugVhandle)[0] << " " << debugMesh.mesh.normal(debugVhandle)[1] << " " << debugMesh.mesh.normal(debugVhandle)[2] << std::endl;
				TEST((mesh.mesh.point(vhandle) - debugMesh.mesh.point(debugVhandle)).length() < 1e-5f, "vertex position match");
				TEST((mesh.mesh.normal(vhandle) - debugMesh.mesh.normal(debugVhandle)).length() < 1e-5f, "vertex normal match");
				//NaniteAssert((mesh.mesh.texcoord2D(vhandle) - debugMesh.mesh.texcoord2D(debugVhandle)).length() < 1e-5f, "vertex texcoord not match");
			}
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
}
