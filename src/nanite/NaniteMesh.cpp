#include "NaniteMesh.h"

#include <queue>

#include "NaniteLodMesh.h"
#include "utils.h"
#include <filesystem>
#include <json.hpp>
#include <OpenMesh/Core/IO/MeshIO.hh>

#include "NaniteBVH.h"

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
		
		clusterIndexOffset.resize(meshes.size(), 0);
		for (size_t i = 0; i < meshes.size(); i++)
		{
			if (i != 0) {
				clusterIndexOffset[i] = clusterIndexOffset[i - 1] + meshes[i - 1].clusterNum;
			}
			meshes[i].createBVH();
		}
		// 展平BVH
		flattenBVH();
	}
	
	void NaniteMesh::flattenBVH()
{
	virtualBVHRootNode = std::make_shared<NaniteBVHNode>();
	virtualBVHRootNode->nodeStatus = NaniteBVHNodeStatus::VIRTUAL_NODE;
	for (const auto & mesh: meshes)
	{
		virtualBVHRootNode->children.push_back(mesh.rootBVHNode);
	}

	std::queue<std::shared_ptr<NaniteBVHNode>> nodeQueue;
	nodeQueue.push(virtualBVHRootNode);

	uint32_t index = 0;

	std::vector<uint32_t> depthCounts;
	uint32_t maxLevels = 0;
	// 两趟BFS遍历：第一趟更新展平索引，第二趟更新子节点索引
	while (!nodeQueue.empty())
	{
		auto currNode = nodeQueue.front();
		currNode->index = index;
		std::string indent(currNode->depth, '\t');
		NaniteAssert(currNode->nodeStatus != NaniteBVHNodeStatus::INVALID, "Invalid node!");

		if (currNode->depth == depthCounts.size())
		{
			depthCounts.push_back(1);
		}
		else
		{
			depthCounts[currNode->depth] += 1;
		}
		index++;
		nodeQueue.pop();
		if (currNode->nodeStatus != NaniteBVHNodeStatus::LEAF)
		{
			for (auto child : currNode->children)
			{
				nodeQueue.push(child);
			}
		}
	}
	flattenedBVHNodeInfos.resize(index); // index 即为节点总数

	uint32_t totalClusterNum = 0;
	for (size_t i = 0; i < meshes.size(); i++)
	{
		totalClusterNum += meshes[i].clusterNum;
	}
	std::unordered_set<uint32_t> clusterIndexSet;
	for (size_t i = 0; i < totalClusterNum; i++)
	{
		clusterIndexSet.insert(i);
	}

	nodeQueue.push(virtualBVHRootNode);
	while (!nodeQueue.empty())
	{
		auto currNode = nodeQueue.front();
		NaniteBVHNodeInfo nodeInfo;
		NaniteAssert(currNode->nodeStatus == VIRTUAL_NODE || currNode->children.size() <= 4, "size of non-virtual nodes' children should never be over 4");
		nodeInfo.children.resize(currNode->children.size());
		for (int i = 0; i < currNode->children.size(); ++i)
		{
			nodeInfo.children[i] = currNode->children[i]->index;
		}
		nodeInfo.pMax = currNode->pMax;
		nodeInfo.pMin = currNode->pMin;
		nodeInfo.parentNormalizedError = currNode->parentNormalizedError;
		nodeInfo.normalizedlodError = currNode->normalizedlodError;
		nodeInfo.parentBoundingSphere = currNode->parentBoundingSphere;
		nodeInfo.nodeStatus = currNode->nodeStatus;
		nodeInfo.depth = currNode->depth;
		nodeInfo.index = currNode->index;
		nodeInfo.clusterIndices = currNode->clusterIndices;
		nodeInfo.lodLevel = currNode->lodLevel;
		NaniteAssert(flattenedBVHNodeInfos[currNode->index].nodeStatus == INVALID, "Repeated index!");
		NaniteAssert(currNode->index < flattenedBVHNodeInfos.size(), "index over flattenedBVHNodeInfos.size()");
		nodeQueue.pop();
		if (currNode->nodeStatus == NaniteBVHNodeStatus::LEAF)
		{
			NaniteAssert(currNode->clusterIndices.size() <= CLUSTER_GROUP_MAX_SIZE, "cluster group size over threshold!");
			int validClusterNum = 0;
			for (size_t i = 0; i < CLUSTER_GROUP_MAX_SIZE; i++)
			{
				auto clusterIndex = currNode->clusterIndices[i];
				if (clusterIndex >= 0) {
					validClusterNum++;
				}
			}
			nodeInfo.start = sortedClusterIndices.size();
			for (size_t i = 0; i < validClusterNum; i++)
			{
				sortedClusterIndices.push_back(currNode->clusterIndices[i] + clusterIndexOffset[currNode->lodLevel]);
				NaniteAssert(clusterIndexSet.find(currNode->clusterIndices[i] + clusterIndexOffset[currNode->lodLevel]) != clusterIndexSet.end(), "Repeated cluster index!");
				clusterIndexSet.erase(currNode->clusterIndices[i] + clusterIndexOffset[currNode->lodLevel]);
			}
			nodeInfo.end = sortedClusterIndices.size();
		}
		else
		{
			for (auto child : currNode->children)
			{
				nodeQueue.push(child);
			}
		}
		flattenedBVHNodeInfos[currNode->index] = nodeInfo;
	}

	NaniteAssert(clusterIndexSet.size() == 0, "Some cluster indices are not assigned to any node!");
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
			// 导出网格到文件
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
		for (size_t i = 0; i < flattenedBVHNodeInfos.size(); i++)
		{
			result["flattenedBVHNodeInfos"][i] = flattenedBVHNodeInfos[i].toJson();
		}
		result["flattenedBVHNodeCounts"] = flattenedBVHNodeInfos.size();
		result["sortedClusterIndices"] = sortedClusterIndices;

		result[cache_time_key] = std::time(nullptr);
		result["lodNums"] = lodNums;

		// 保存JSON数据到文件
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
		
		int flattenedBVHNodeCounts = loadedJson["flattenedBVHNodeCounts"].get<uint32_t>();
		flattenedBVHNodeInfos.resize(flattenedBVHNodeCounts, NaniteBVHNodeInfo());
		for (int i = 0; i < flattenedBVHNodeCounts; ++i) {
			auto& nodeInfo = flattenedBVHNodeInfos[i];
			nodeInfo.fromJson(loadedJson["flattenedBVHNodeInfos"][i]);
			float percentage = static_cast<float>(i + 1) / flattenedBVHNodeCounts * 100.0;
			std::cout << "\r";
			std::cout << "[Loading] BVH Info: " << std::fixed << std::setw(6) << std::setprecision(2) << percentage << "%";
			std::cout.flush();
		}
		std::cout << std::endl;

		for (size_t i = 0; i < loadedJson["sortedClusterIndices"].size(); i++)
		{
			sortedClusterIndices.push_back(loadedJson["sortedClusterIndices"][i].get<uint32_t>());
		}

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

	void NaniteMesh::buildClusterInfo()
	{
	// 初始化 Cluster 信息
	size_t totalClusterNum = 0;
	for (int i = 0; i < meshes.size(); i++)
	{
		totalClusterNum += meshes[i].clusterNum;
#ifdef DEBUG_LOD_START
		break;
#endif // DEBUG_LOD_START
	}
	clusterInfo.resize(totalClusterNum);
	errorInfo.resize(totalClusterNum);
	size_t currClusterNum = 0, currTriangleNum = 0;
	for (int i = 0; i < meshes.size(); i++)
	{
		auto& mesh = meshes[i].mesh;
		for (NaniteTriMesh::FaceIter face_it = mesh.faces_begin(); face_it != mesh.faces_end(); ++face_it) {
			NaniteTriMesh::FaceHandle fh = *face_it;
			auto clusterIdx = meshes[i].triangleClusterIndex[fh.idx()] + currClusterNum;
			auto& clusterI = clusterInfo[clusterIdx];

			glm::vec3 pMinWorld, pMaxWorld;
			glm::vec3 p0, p1, p2;
			NaniteTriMesh::FaceVertexIter fv_it = mesh.fv_iter(fh);

			// 获取三角形三个顶点的位置
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

			p0 = glm::vec3(glm::vec4(p0, 1.0f));
			p1 = glm::vec3(glm::vec4(p1, 1.0f));
			p2 = glm::vec3(glm::vec4(p2, 1.0f));

			getTriangleAABB(p0, p1, p2, pMinWorld, pMaxWorld);

			clusterI.mergeAABB(pMinWorld, pMaxWorld);
		}


		uint32_t currClusterIdx = -1;
		for (size_t j = 0; j < meshes[i].triangleIndicesSortedByClusterIdx.size(); j++)
		{
			auto currTriangleIndex = meshes[i].triangleIndicesSortedByClusterIdx[j];
			if (meshes[i].triangleClusterIndex[currTriangleIndex] != currClusterIdx)
			{
				if (currClusterIdx != -1) {
					clusterInfo[currClusterIdx + currClusterNum].triangleIndicesEnd = j + currTriangleNum;
				}
				currClusterIdx = meshes[i].triangleClusterIndex[currTriangleIndex];
				clusterInfo[currClusterIdx + currClusterNum].triangleIndicesStart = j + currTriangleNum;
			}
		}
		clusterInfo[currClusterIdx + currClusterNum].triangleIndicesEnd = meshes[i].triangleIndicesSortedByClusterIdx.size() + currTriangleNum;

		for (size_t j = 0; j < meshes[i].clusters.size(); j++)
		{
			auto& cluster = meshes[i].clusters[j];
			float parentError = i == meshes.size() - 1 ? 1e5 : cluster.parentNormalizedError;
			NaniteAssert(parentError > cluster.normalizedlodError, "Parent error is not greater than children's");
			errorInfo[j + currClusterNum].errorWorld = glm::vec2(cluster.normalizedlodError, parentError);
			glm::vec3 worldCenter = glm::vec3(glm::vec4(cluster.boundingSphereCenter, 1.0));
			float worldRadius = glm::length(glm::vec4(glm::vec3(cluster.boundingSphereRadius, 0, 0), 0.0));
			NaniteAssert(cluster.triangleIndices.size() <= CLUSTER_MAX_SIZE, "cluster.triangleIndices.size() is over thresold");
			NaniteAssert(cluster.boundingSphereRadius > 0 || cluster.triangleIndices.size() == 0, "boundingSphereRadius <= 0");
			NaniteAssert(worldRadius > 0 || cluster.triangleIndices.size() == 0, "worldRadius <= 0");
			errorInfo[j + currClusterNum].centerR = glm::vec4(worldCenter, worldRadius);
			float parentBoundingRadius = 0;
			glm::vec3 parentCenter = glm::vec3(0);
			if (i == meshes.size() - 1) // 最后一级LOD，无父节点
			{
				parentBoundingRadius = worldRadius * 1.5f;
				parentCenter = cluster.boundingSphereCenter;
			}
			else for (size_t k : cluster.parentClusterIndices) // 获取最大父节点包围球尺寸
			{
				parentBoundingRadius = std::max(parentBoundingRadius, meshes[i + 1].clusters[k].boundingSphereRadius);
				parentCenter += meshes[i + 1].clusters[k].boundingSphereCenter;
				break;
			}
			glm::vec3 parentWorldCenter = glm::vec3(glm::vec4(parentCenter, 1.0));
			errorInfo[j + currClusterNum].centerRP = glm::vec4(parentWorldCenter, parentBoundingRadius);
		}
		currClusterNum += meshes[i].clusterNum;
		currTriangleNum += meshes[i].triangleIndicesSortedByClusterIdx.size();
#ifdef DEBUG_LOD_START
		break;
#endif // DEBUG_LOD_START
	}
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
			// TODO: 检查缓存时间戳以判断是否需要重新生成
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
			generateNaniteInfo();
			serialize(cachePath);
			std::cout << cachePath << "nanite_info.json" << " generated" << std::endl;
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
			NaniteAssert(mesh.clusters.size() == debugMesh.clusters.size(), "cluster size match");
			for (size_t clusterIdx = 0; clusterIdx < mesh.clusters.size(); clusterIdx++)
			{
				const auto& cluster = mesh.clusters[clusterIdx];
				const auto& debugCluster = debugMesh.clusters[clusterIdx];
				NaniteAssert(cluster.parentNormalizedError == debugCluster.parentNormalizedError, "parentNormalizedError match");
				NaniteAssert(cluster.lodError == debugCluster.lodError, "lodError match");
				NaniteAssert(cluster.boundingSphereCenter == debugCluster.boundingSphereCenter, "boundingSphereCenter match");
				NaniteAssert(cluster.boundingSphereRadius == debugCluster.boundingSphereRadius, "boundingSphereRadius match");
			}
			NaniteAssert(mesh.mesh.n_faces() == debugMesh.mesh.n_faces(), "face size match");
			NaniteAssert(mesh.mesh.n_vertices() == debugMesh.mesh.n_vertices(), "vertex size match");
			for (const auto& vhandle : mesh.mesh.vertices())
			{
				auto debugVhandle = debugMesh.mesh.vertex_handle(vhandle.idx());
				NaniteAssert((mesh.mesh.point(vhandle) - debugMesh.mesh.point(debugVhandle)).length() < 1e-5f, "vertex position match");
				NaniteAssert((mesh.mesh.normal(vhandle) - debugMesh.mesh.normal(debugVhandle)).length() < 1e-5f, "vertex normal match");
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
