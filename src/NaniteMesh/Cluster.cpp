#include "Cluster.h"

#include "../utils.h"

namespace Nanite
{
	nlohmann::json Cluster::toJson()
	{
		return {{"normalizedlodError", normalizedlodError}, {"parentNormalizedError", parentNormalizedError}, {"lodError", lodError}, {"boundingSphereCenter", {boundingSphereCenter.x, boundingSphereCenter.y, boundingSphereCenter.z}}, {"boundingSphereRadius", boundingSphereRadius}, {"parentClusterIndices", parentClusterIndices}, {"triangleIndices", triangleIndices}};
	}

	void Cluster::fromJson(const nlohmann::json& data)
	{
		NaniteAssert(data.find("normalizedlodError") != data.end(), "normalizedlodError not found");
		normalizedlodError = data["normalizedlodError"].get<double>();

		NaniteAssert(data.find("parentNormalizedError") != data.end(), "parentNormalizedError not found");
		parentNormalizedError = data["parentNormalizedError"].get<double>();

		NaniteAssert(data.find("lodError") != data.end(), "lodError not found");
		lodError = data["lodError"].get<double>();

		NaniteAssert(data.find("boundingSphereCenter") != data.end() && data["boundingSphereCenter"].is_array() && data["boundingSphereCenter"].size() == 3, "boundingSphereCenter not found or not properly set");
		if (data.find("boundingSphereCenter") != data.end() && data["boundingSphereCenter"].is_array() && data["boundingSphereCenter"].size() == 3)
		{
			boundingSphereCenter.x = data["boundingSphereCenter"][0].get<float>();
			boundingSphereCenter.y = data["boundingSphereCenter"][1].get<float>();
			boundingSphereCenter.z = data["boundingSphereCenter"][2].get<float>();
		}

		NaniteAssert(data.find("boundingSphereRadius") != data.end(), "boundingSphereRadius not found");

		boundingSphereRadius = data["boundingSphereRadius"].get<double>();

		NaniteAssert(data.find("parentClusterIndices") != data.end() && data["parentClusterIndices"].is_array(), "parentClusterIndices not found");
		for (auto& idx : data["parentClusterIndices"])
		{
			parentClusterIndices.emplace_back(idx.get<uint32_t>());
		}
		NaniteAssert(data.find("triangleIndices") != data.end() && data["triangleIndices"].is_array(), "triangleIndices not found");
		for (auto& idx : data["triangleIndices"])
		{
			triangleIndices.emplace_back(idx.get<uint32_t>());
		}
	}

	nlohmann::json ClusterNode::toJson()
	{
		return {{"parentMaxLODError", parentMaxLODError}, {"lodError", lodError}, {"boundingSphereCenter", {boundingSphereCenter.x, boundingSphereCenter.y, boundingSphereCenter.z}}, {"boundingSphereRadius", boundingSphereRadius}};
	}

	void ClusterNode::fromJson(const nlohmann::json& data)
	{
		if (data.find("parentMaxLODError") != data.end())
		{
			parentMaxLODError = data["parentMaxLODError"].get<double>();
		}

		if (data.find("lodError") != data.end())
		{
			lodError = data["lodError"].get<double>();
		}

		if (data.find("boundingSphereCenter") != data.end() && data["boundingSphereCenter"].is_array() && data["boundingSphereCenter"].size() == 3)
		{
			boundingSphereCenter.x = data["boundingSphereCenter"][0].get<float>();
			boundingSphereCenter.y = data["boundingSphereCenter"][1].get<float>();
			boundingSphereCenter.z = data["boundingSphereCenter"][2].get<float>();
		}

		if (data.find("boundingSphereRadius") != data.end())
		{
			boundingSphereRadius = data["boundingSphereRadius"].get<double>();
		}
	}
}
