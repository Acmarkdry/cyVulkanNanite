#include "Const.h"

void Nanite::Graph::resize(uint32_t newSize)
{
	adjMap.resize(newSize);
}

void Nanite::Graph::addEdge(uint32_t from, uint32_t to, int cost)
{
	adjMap[from][to] = cost;
}

void Nanite::Graph::addEdgeCost(uint32_t from, uint32_t to, int cost)
{
	adjMap[from][to] += cost;
}

Nanite::MetisGraph Nanite::MetisGraph::GraphToMetisGraph(const Graph& graph)
{
	MetisGraph metisGraph;
	metisGraph.nvtxs = graph.adjMap.size();

	for (auto& adjList : graph.adjMap)
	{
		metisGraph.xadj.push_back(metisGraph.adjncy.size());
		for (const auto& toAndCost : adjList)
		{
			metisGraph.adjncy.push_back(toAndCost.first);
			metisGraph.adjwgt.push_back(toAndCost.second);
		}
	}
	metisGraph.xadj.push_back(metisGraph.adjncy.size());

	return metisGraph;
}
