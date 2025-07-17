#include "ClusterGroup.h"

void Nanite::ClusterGroup::buildTriangleIndicesLocalGlobalMapping()
{
}

void Nanite::ClusterGroup::buildLocalTriangleGraph()
{
}

void Nanite::ClusterGroup::generateLocalClusters()
{
}

void Nanite::ClusterGroup::mergeAABB(const glm::vec3& pMinOther, const glm::vec3& pMaxOther)
{
	pMin = glm::min(pMin, pMinOther);
	pMax = glm::max(pMax, pMaxOther);
}
