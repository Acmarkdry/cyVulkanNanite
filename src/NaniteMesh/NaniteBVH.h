#pragma once

#include <json.hpp>

#include "../utils.h"

namespace Nanite
{
enum NaniteBVHNodeStatus 
{
	INVALID,      // 默认初始化状态
	VIRTUAL_NODE, // 没有包围盒的虚拟节点，仅用于实现便利
	NODE,         // 所有有效的非叶节点
	LEAF          // 存储了 cluster 索引的叶节点
};

// 存储所有子节点的指针，树形结构
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
			objectIdx = -1;  // 仅在实例化时使用
			lodLevel = -1;
			for (size_t i = 0; i < CLUSTER_GROUP_MAX_SIZE; i++) clusterIndices[i] = -1;
		}
	std::vector<std::shared_ptr<NaniteBVHNode>> children;
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
	
	// 仅叶节点会填充 clusterIndices
	std::array<int, CLUSTER_GROUP_MAX_SIZE> clusterIndices;
	
	int lodLevel = -1; 
	int objectIdx = -1;  // 仅在实例化时使用
	int meshIdx = -1;    // 仅在实例化时使用
};

// 存储所有子节点的索引，数组结构（用于展平后的BVH）
struct NaniteBVHNodeInfo
{
	double normalizedlodError = -FLT_MAX;
	double parentNormalizedError = -FLT_MAX;
	glm::vec4 parentBoundingSphere;
	int index = -1;
	glm::vec3 pMin = glm::vec3(FLT_MAX);
	glm::vec3 pMax = glm::vec3(-FLT_MAX);
	std::vector<int> children;
	std::array<int, CLUSTER_GROUP_MAX_SIZE> clusterIndices;
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
