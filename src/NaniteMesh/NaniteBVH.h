#pragma once

#include <json.hpp>

#include "../utils.h"

namespace Nanite
{
enum NaniteBVHNodeStatus 
{
	INVALID, // Default initialization
	VIRTUAL_NODE, // All nodes that do not have a bounding box, only for implementation convenience
	NODE, // All valid nodes
	LEAF  // All nodes that actually stores cluster indices
};

/*
	Stores the **pointers** to all children nodes
	Tree structure
*/

struct NaniteBVHNode
{
	NaniteBVHNode():
		normalizedlodError(-FLT_MAX),
		parentNormalizedError(-FLT_MAX),
		index(-1),
		pMin(glm::vec3(FLT_MAX)),
		pMax(glm::vec3(-FLT_MAX)), 
		nodeStatus(INVALID),
		start(-1),
		end(-1),
		depth(0)
		{
			objectIdx = -1; // Only useful in instancing
			lodLevel = -1;
			for (size_t i = 0; i < CLUSTER_GROUP_MAX_SIZE; i++) clusterIndices[i] = -1; // The non-default initialization
		}
	std::vector<std::shared_ptr<NaniteBVHNode>> children; // should be a fixed size
	double normalizedlodError = -FLT_MAX;
	double parentNormalizedError = -FLT_MAX;
	glm::vec4 parentBoundingSphere;
	int index = -1;
	glm::vec3 pMin = glm::vec3(FLT_MAX);
	glm::vec3 pMax = glm::vec3(-FLT_MAX);
	NaniteBVHNodeStatus nodeStatus = NaniteBVHNodeStatus::INVALID;
	uint32_t start = -1;
	uint32_t end = -1;
	uint32_t depth = 0;
	
	// Only leaf node will have `clusterIndices` filled
	//std::vector<uint32_t> clusterIndices; 
	std::array<int, CLUSTER_GROUP_MAX_SIZE> clusterIndices; // should try to NaniteAssert index overflow
	
	int lodLevel = -1; 
	int objectIdx = -1; // Only useful in instancing
	int meshIdx = -1; // Only useful in instancing
};

/*
	Stores the **indices** to all children nodes
	Array structure
*/

struct NaniteBVHNodeInfo
{
	double normalizedlodError = -FLT_MAX;
	double parentNormalizedError = -FLT_MAX;
	glm::vec4 parentBoundingSphere;
	int index = -1;
	glm::vec3 pMin = glm::vec3(FLT_MAX);
	glm::vec3 pMax = glm::vec3(-FLT_MAX);
	std::vector<int> children;
	//glm::ivec4 children = glm::ivec4(-1);
	std::array<int, CLUSTER_GROUP_MAX_SIZE> clusterIndices; // should try to NaniteAssert index overflow
	int start = -1;
	int end = -1;
	NaniteBVHNodeStatus nodeStatus = NaniteBVHNodeStatus::INVALID;
	uint32_t depth = 0;
	int lodLevel = -1;

	nlohmann::json toJson() {
		return {
			{"normalizedlodError", normalizedlodError},
			{"parentNormalizedError", parentNormalizedError},
			{"parentBoundingSphere", {parentBoundingSphere.x, parentBoundingSphere.y, parentBoundingSphere.z, parentBoundingSphere.w}},
			{"index", index},
			{"pMin", {pMin.x, pMin.y, pMin.z}},
			{"pMax", {pMax.x, pMax.y, pMax.z}},
			{"children", children},
			{"clusterIndices", clusterIndices},
			{"start", start},
			{"end", end},
			{"nodeStatus", nodeStatus},
			{"depth", depth},
			{"lodLevel", lodLevel}
		};
	}

	void fromJson(const nlohmann::json& j) 
	{
		NaniteAssert(j.find("normalizedlodError") != j.end(), "normalizedlodError not found");
		normalizedlodError = j["normalizedlodError"].get<double>();

		NaniteAssert(j.find("parentNormalizedError") != j.end(), "parentNormalizedError not found");
		parentNormalizedError = j["parentNormalizedError"].get<double>();

		NaniteAssert(j.find("parentBoundingSphere") != j.end() && j["parentBoundingSphere"].is_array() && j["parentBoundingSphere"].size() == 4,
			"parentBoundingSphere not found or not properly set");
		if (j.find("parentBoundingSphere") != j.end() && j["parentBoundingSphere"].is_array() && j["parentBoundingSphere"].size() == 4) {
			parentBoundingSphere.x = j["parentBoundingSphere"][0].get<float>();
			parentBoundingSphere.y = j["parentBoundingSphere"][1].get<float>();
			parentBoundingSphere.z = j["parentBoundingSphere"][2].get<float>();
			parentBoundingSphere.w = j["parentBoundingSphere"][3].get<float>();
		}

		NaniteAssert(j.find("index") != j.end(), "index not found");
		index = j["index"].get<int>();

		NaniteAssert(j.find("pMin") != j.end() && j["pMin"].is_array() && j["pMin"].size() == 3,
			"pMin not found or not properly set");
		if (j.find("pMin") != j.end() && j["pMin"].is_array() && j["pMin"].size() == 3) {
			pMin.x = j["pMin"][0].get<float>();
			pMin.y = j["pMin"][1].get<float>();
			pMin.z = j["pMin"][2].get<float>();
		}

		NaniteAssert(j.find("pMax") != j.end() && j["pMax"].is_array() && j["pMax"].size() == 3,
			"pMax not found or not properly set");
		if (j.find("pMax") != j.end() && j["pMax"].is_array() && j["pMax"].size() == 3) {
			pMax.x = j["pMax"][0].get<float>();
			pMax.y = j["pMax"][1].get<float>();
			pMax.z = j["pMax"][2].get<float>();
		}

		NaniteAssert(j.find("children") != j.end() && j["children"].is_array(), "children not found");
		children.resize(j["children"].size());
		for (int i = 0; i < j["children"].size(); ++i) {
			children[i] = j["children"].at(i).get<uint32_t>();
		}

		NaniteAssert(j.find("start") != j.end(), "start not found");
		start = j["start"].get<int>();

		NaniteAssert(j.find("end") != j.end(), "end not found");
		end = j["end"].get<int>();

		NaniteAssert(j.find("clusterIndices") != j.end() && j["clusterIndices"].is_array(), "clusterIndices not found");
		for (int i = 0; i < CLUSTER_GROUP_MAX_SIZE; ++i) {
			clusterIndices[i] = j["clusterIndices"].at(i).get<uint32_t>();
		}

		NaniteAssert(j.find("nodeStatus") != j.end(), "nodeStatus not found");
		nodeStatus = j["nodeStatus"].get<NaniteBVHNodeStatus>();

		NaniteAssert(j.find("depth") != j.end(), "depth not found");
		depth = j["depth"].get<uint32_t>();

		NaniteAssert(j.find("lodLevel") != j.end(), "lodLevel not found");
		lodLevel = j["lodLevel"].get<int>();
	}
};
	
}
