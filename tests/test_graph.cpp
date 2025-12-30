// tests/test_graph.cpp
// Graph 与 MetisGraph 数据结构属性测试
// 验证需求 5.1–5.5

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include "Const.h"

// ============================================================
// 6.1  属性测试：Graph::addEdge 记录边（属性 4）
// Feature: nanite-testing, Property 4: Graph::addEdge 记录边
// **Validates: Requirements 5.1**
//
// 对于任意有效的图大小 n、顶点对 (from, to) 其中 from,to < n、
// 以及权重 cost，调用 addEdge(from, to, cost) 后，
// adjMap[from][to] 应等于 cost。
// ============================================================
RC_GTEST_PROP(GraphPropertyTest, AddEdgeRecordsEdge, ())
{
    const auto n = *rc::gen::inRange(1, 101);
    const auto from = *rc::gen::inRange(0, n);
    const auto to = *rc::gen::inRange(0, n);
    const auto cost = *rc::gen::inRange(-10000, 10001);

    Nanite::Graph graph;
    graph.resize(static_cast<uint32_t>(n));
    graph.addEdge(static_cast<uint32_t>(from), static_cast<uint32_t>(to), cost);

    RC_ASSERT(graph.adjMap[from][to] == cost);
}

// ============================================================
// 6.2  属性测试：Graph::addEdgeCost 累加权重（属性 5）
// Feature: nanite-testing, Property 5: Graph::addEdgeCost 累加权重
// **Validates: Requirements 5.2**
//
// 对于任意已存在权重为 w1 的边 (from, to)，调用
// addEdgeCost(from, to, w2) 后，adjMap[from][to] 应等于 w1 + w2。
// ============================================================
RC_GTEST_PROP(GraphPropertyTest, AddEdgeCostAccumulatesWeight, ())
{
    const auto n = *rc::gen::inRange(1, 101);
    const auto from = *rc::gen::inRange(0, n);
    const auto to = *rc::gen::inRange(0, n);
    const auto w1 = *rc::gen::inRange(-10000, 10001);
    const auto w2 = *rc::gen::inRange(-10000, 10001);

    Nanite::Graph graph;
    graph.resize(static_cast<uint32_t>(n));
    graph.addEdge(static_cast<uint32_t>(from), static_cast<uint32_t>(to), w1);
    graph.addEdgeCost(static_cast<uint32_t>(from), static_cast<uint32_t>(to), w2);

    RC_ASSERT(graph.adjMap[from][to] == w1 + w2);
}

// ============================================================
// 6.3  属性测试：GraphToMetisGraph CSR 格式有效性（属性 6）
// Feature: nanite-testing, Property 6: GraphToMetisGraph CSR 格式有效性
// **Validates: Requirements 5.3, 5.4, 5.5**
//
// 对于任意有效的 Graph 对象，调用 GraphToMetisGraph 后：
// (1) xadj.size() == nvtxs + 1
// (2) adjncy.size() == adjwgt.size()
// (3) 对于每个顶点 v，xadj[v+1] - xadj[v] 等于该顶点在原图中的邻居数量
// ============================================================
RC_GTEST_PROP(GraphPropertyTest, GraphToMetisGraphCSRValidity, ())
{
    // 生成随机图：n 个顶点，随机边
    const auto n = *rc::gen::inRange(1, 51);

    Nanite::Graph graph;
    graph.resize(static_cast<uint32_t>(n));

    // 随机添加若干条边
    const auto edgeCount = *rc::gen::inRange(0, n * n / 2 + 1);
    for (int e = 0; e < edgeCount; ++e)
    {
        const auto from = *rc::gen::inRange(0, n);
        const auto to = *rc::gen::inRange(0, n);
        const auto cost = *rc::gen::inRange(1, 101);
        graph.addEdge(static_cast<uint32_t>(from), static_cast<uint32_t>(to), cost);
    }

    const auto metis = Nanite::MetisGraph::GraphToMetisGraph(graph);

    // (1) xadj 长度 == nvtxs + 1
    RC_ASSERT(static_cast<int>(metis.xadj.size()) == metis.nvtxs + 1);

    // (2) adjncy 和 adjwgt 长度相等
    RC_ASSERT(metis.adjncy.size() == metis.adjwgt.size());

    // (3) 每个顶点的邻居数量一致
    for (int v = 0; v < metis.nvtxs; ++v)
    {
        const auto csrNeighborCount = metis.xadj[v + 1] - metis.xadj[v];
        const auto originalNeighborCount = static_cast<idx_t>(graph.adjMap[v].size());
        RC_ASSERT(csrNeighborCount == originalNeighborCount);
    }
}
