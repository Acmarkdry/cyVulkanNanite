// tests/test_parallelfor.cpp
// ParallelFor 正确性测试 — 单元测试 + 属性测试
// 验证需求 4.1–4.3, 4.5

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

#include "TaskGraph.h"

// ============================================================
// 辅助：每个测试前初始化 TaskGraph，测试后关闭
// ============================================================
class ParallelForTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Nanite::TaskGraph::Get().Initialize(4);
    }

    void TearDown() override
    {
        Nanite::TaskGraph::Get().Shutdown();
    }
};

// ============================================================
// 5.1  ParallelFor num<=0 安全返回（需求 4.2）
// ============================================================
TEST_F(ParallelForTest, NumZeroDoesNotExecuteBody)
{
    std::atomic<int> counter{0};
    Nanite::ParallelFor(0, [&counter](int32_t) {
        counter.fetch_add(1, std::memory_order_relaxed);
    });
    EXPECT_EQ(counter.load(), 0);
}

TEST_F(ParallelForTest, NumNegativeDoesNotExecuteBody)
{
    std::atomic<int> counter{0};
    Nanite::ParallelFor(-5, [&counter](int32_t) {
        counter.fetch_add(1, std::memory_order_relaxed);
    });
    EXPECT_EQ(counter.load(), 0);
}

// ============================================================
// 5.2  forceSingleThread 模式（需求 4.3）
// 当 forceSingleThread=true 时，所有迭代在调用线程上执行
// ============================================================
TEST_F(ParallelForTest, ForceSingleThreadRunsOnCallingThread)
{
    constexpr int32_t NUM = 50;
    const auto callingThreadId = std::this_thread::get_id();

    std::vector<std::thread::id> threadIds(NUM);

    Nanite::ParallelFor(NUM, [&threadIds](int32_t index) {
        threadIds[index] = std::this_thread::get_id();
    }, /*forceSingleThread=*/true);

    for (int32_t i = 0; i < NUM; ++i)
    {
        EXPECT_EQ(threadIds[i], callingThreadId)
            << "Index " << i << " ran on a different thread";
    }
}

// ============================================================
// RapidCheck 属性测试
// ============================================================
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

// ============================================================
// 5.3  属性测试：ParallelFor 并行与顺序结果一致（属性 3）
// Feature: nanite-testing, Property 3: ParallelFor 并行与顺序结果一致
// **Validates: Requirements 4.1, 4.5**
//
// 对于任意正整数 num，ParallelFor 应对 [0, num) 中的
// 每个索引恰好执行一次 body 函数。
// ============================================================
RC_GTEST_PROP(ParallelForPropertyTest, EachIndexExecutedExactlyOnce, ())
{
    const auto num = *rc::gen::inRange(1, 1001);

    Nanite::TaskGraph::Get().Initialize(4);

    // 原子数组：记录每个索引的执行次数
    std::vector<std::atomic<int>> counts(num);
    for (auto& c : counts)
        c.store(0, std::memory_order_relaxed);

    Nanite::ParallelFor(static_cast<int32_t>(num), [&counts](int32_t index) {
        counts[index].fetch_add(1, std::memory_order_relaxed);
    });

    // 验证每个索引恰好执行一次
    for (int i = 0; i < num; ++i)
    {
        RC_ASSERT(counts[i].load(std::memory_order_acquire) == 1);
    }

    Nanite::TaskGraph::Get().Shutdown();
}
