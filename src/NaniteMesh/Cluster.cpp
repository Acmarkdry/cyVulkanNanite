#include "Cluster.h"

namespace Nanite
{
	nlohmann::json ClusterNode::toJson()
	{
		return {
			{"parentMaxLodError",parentMaxLODError},
			{"lodError",lodError},
			{"boundingSphereCenter",{boundingSphereCenter.x, boundingSphereCenter.y, boundingSphereCenter.z}},
			{"boundingSphereRadius",boundingSphereRadius},
		};
	}

	void ClusterNode::fromJson(const nlohmann::json& data)
	{
		if (data.find("parentMaxLodError") != data.end())
		{
			parentMaxLODError = data["parentMaxLodError"].get<double>();
		}
		if (data.find("lodError") != data.end())
		{
			lodError = data["lodError"].get<double>();
		}
		if (data.find("boundingSphereCenter") != data.end() && data["boundingSphereCenter"].is_array())
		{
			boundingSphereCenter.x = data["boundingSphereCenter"]["x"].get<double>();
			boundingSphereCenter.y = data["boundingSphereCenter"]["y"].get<double>();
			boundingSphereCenter.z = data["boundingSphereCenter"]["z"].get<double>();
		}
		if (data.find("boundingSphereRadius") != data.end())
		{
			boundingSphereRadius = data["boundingSphereRadius"].get<double>();
		}
	
	}	
}

