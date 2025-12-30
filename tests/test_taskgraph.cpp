// tests/test_taskgraph.cpp
// TaskGraph 单元测试 — 生命周期管理与任务依赖调度
// 验证需求 2.1–2.5, 3.1, 3.4, 3.5, 3.6

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "TaskGraph.h"

// ============================================================
// 辅助：每个测试结束后确保 TaskGraph 处于干净状态
// ============================================================
class TaskGraphTest : public ::testing::Test
{
protected:
    void TearDown() override
    {
        Nanite::TaskGraph::Get().Shutdown();
    }
};

// ============================================================
// 3.1  Initialize(N) 后 GetWorkerCount() == N（需求 2.1）
// ============================================================
TEST_F(TaskGraphTest, InitializeWithExplicitWorkerCount)
{
    constexpr uint32_t N = 4;
    Nanite::TaskGraph::Get().Initialize(N);
    EXPECT_EQ(Nanite::TaskGraph::Get().GetWorkerCount(), N);
}

// ============================================================
// 3.2  Initialize(0) 使用 hardware_concurrency（需求 2.2）
// ============================================================
TEST_F(TaskGraphTest, InitializeZeroUsesHardwareConcurrency)
{
    Nanite::TaskGraph::Get().Initialize(0);

    const uint32_t expected = std::max(1u, std::thread::hardware_concurrency());
    EXPECT_EQ(Nanite::TaskGraph::Get().GetWorkerCount(), expected);
}

// ============================================================
// 3.3  Shutdown 后资源释放（需求 2.3）
// ============================================================
TEST_F(TaskGraphTest, ShutdownReleasesWorkers)
{
    Nanite::TaskGraph::Get().Initialize(4);
    EXPECT_EQ(Nanite::TaskGraph::Get().GetWorkerCount(), 4u);

    Nanite::TaskGraph::Get().Shutdown();
    // Shutdown 清空 workers 向量，GetWorkerCount() 应为 0
    EXPECT_EQ(Nanite::TaskGraph::Get().GetWorkerCount(), 0u);
}

// ============================================================
// 3.4  重复 Initialize 幂等性（需求 2.4）
// ============================================================
TEST_F(TaskGraphTest, RepeatedInitializeIsIdempotent)
{
    Nanite::TaskGraph::Get().Initialize(4);
    EXPECT_EQ(Nanite::TaskGraph::Get().GetWorkerCount(), 4u);

    // 第二次 Initialize 应被忽略，worker 数量不变
    Nanite::TaskGraph::Get().Initialize(8);
    EXPECT_EQ(Nanite::TaskGraph::Get().GetWorkerCount(), 4u);
}

// ============================================================
// 3.5  未初始化时 Shutdown 安全返回（需求 2.5）
// ============================================================
TEST_F(TaskGraphTest, ShutdownWithoutInitializeIsSafe)
{
    // 不调用 Initialize，直接 Shutdown 不应崩溃
    EXPECT_NO_FATAL_FAILURE(Nanite::TaskGraph::Get().Shutdown());
}

// ============================================================
// 3.6  无依赖任务立即执行（需求 3.1）
// ============================================================
TEST_F(TaskGraphTest, TaskWithoutPrerequisitesExecutesImmediately)
{
    Nanite::TaskGraph::Get().Initialize(4);

    std::atomic<bool> executed{false};

    auto event = Nanite::TaskGraph::Get().CreateAndDispatchTask(
        [&executed]() { executed.store(true, std::memory_order_release); });

    event->Wait();
    EXPECT_TRUE(executed.load(std::memory_order_acquire));
    EXPECT_TRUE(event->IsComplete());
}

// ============================================================
// 3.7  多任务依赖同一事件的扇出（需求 3.4）
// ============================================================
TEST_F(TaskGraphTest, FanOutMultipleTasksDependOnSameEvent)
{
    Nanite::TaskGraph::Get().Initialize(4);

    // 创建一个前置任务
    std::atomic<int> counter{0};

    auto prerequisite = Nanite::TaskGraph::Get().CreateAndDispatchTask(
        []() { /* 前置任务，什么也不做 */ });

    // 创建 3 个后续任务，都依赖同一个前置事件
    constexpr int FAN_OUT = 3;
    std::vector<Nanite::GraphEvent::Ptr> fanOutEvents;
    fanOutEvents.reserve(FAN_OUT);

    for (int i = 0; i < FAN_OUT; ++i)
    {
        fanOutEvents.push_back(
            Nanite::TaskGraph::Get().CreateAndDispatchTask(
                [&counter]() {
                    counter.fetch_add(1, std::memory_order_acq_rel);
                },
                {prerequisite}));
    }

    // 等待所有扇出任务完成
    Nanite::TaskGraph::WaitForEvents(fanOutEvents);

    EXPECT_EQ(counter.load(std::memory_order_acquire), FAN_OUT);
    for (const auto& ev : fanOutEvents)
    {
        EXPECT_TRUE(ev->IsComplete());
    }
}

// ============================================================
// 3.8  已完成事件 Wait 立即返回（需求 3.5）
// ============================================================
TEST_F(TaskGraphTest, WaitOnCompletedEventReturnsImmediately)
{
    Nanite::TaskGraph::Get().Initialize(4);

    auto event = Nanite::TaskGraph::Get().CreateAndDispatchTask(
        []() { /* 空任务 */ });

    // 先等待任务完成
    event->Wait();
    ASSERT_TRUE(event->IsComplete());

    // 再次 Wait 应立即返回，不阻塞
    auto start = std::chrono::steady_clock::now();
    event->Wait();
    auto elapsed = std::chrono::steady_clock::now() - start;

    // 立即返回应在 1ms 以内
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 100);
}

// ============================================================
// 3.9  依赖链 A→B→C 执行顺序（需求 3.6）
// ============================================================
TEST_F(TaskGraphTest, DependencyChainABC_ExecutionOrder)
{
    Nanite::TaskGraph::Get().Initialize(4);

    // 使用时间戳记录每个任务的完成顺序
    std::atomic<int> orderCounter{0};
    std::atomic<int> orderA{-1};
    std::atomic<int> orderB{-1};
    std::atomic<int> orderC{-1};

    // 任务 A：无依赖
    auto eventA = Nanite::TaskGraph::Get().CreateAndDispatchTask(
        [&]() {
            orderA.store(orderCounter.fetch_add(1, std::memory_order_acq_rel),
                         std::memory_order_release);
        });

    // 任务 B：依赖 A
    auto eventB = Nanite::TaskGraph::Get().CreateAndDispatchTask(
        [&]() {
            orderB.store(orderCounter.fetch_add(1, std::memory_order_acq_rel),
                         std::memory_order_release);
        },
        {eventA});

    // 任务 C：依赖 B
    auto eventC = Nanite::TaskGraph::Get().CreateAndDispatchTask(
        [&]() {
            orderC.store(orderCounter.fetch_add(1, std::memory_order_acq_rel),
                         std::memory_order_release);
        },
        {eventB});

    eventC->Wait();

    EXPECT_TRUE(eventA->IsComplete());
    EXPECT_TRUE(eventB->IsComplete());
    EXPECT_TRUE(eventC->IsComplete());

    // A 在 B 之前，B 在 C 之前
    EXPECT_LT(orderA.load(), orderB.load());
    EXPECT_LT(orderB.load(), orderC.load());
}

// ============================================================
// RapidCheck 属性测试
// ============================================================
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

// ============================================================
// 4.1  属性测试：任务依赖执行顺序（属性 1）
// Feature: nanite-testing, Property 1: 任务依赖执行顺序
// **Validates: Requirements 3.2, 3.6**
//
// 对于任意长度的依赖链，前置任务的完成时间戳应始终
// 小于后续任务的时间戳（单调递增）。
// ============================================================
RC_GTEST_PROP(TaskGraphPropertyTest, DependencyChainTimestampsAreMonotonic, ())
{
    // 生成链长度 [2, 10]
    const auto chainLength = *rc::gen::inRange(2, 11);

    Nanite::TaskGraph::Get().Initialize(4);

    // 原子计数器用作逻辑时间戳
    std::atomic<int> timestamp{0};
    std::vector<std::atomic<int>> completionOrder(chainLength);
    for (auto& v : completionOrder)
        v.store(-1, std::memory_order_relaxed);

    // 构建依赖链：task[0] → task[1] → ... → task[chainLength-1]
    std::vector<Nanite::GraphEvent::Ptr> events;
    events.reserve(chainLength);

    for (int i = 0; i < chainLength; ++i)
    {
        std::vector<Nanite::GraphEvent::Ptr> prereqs;
        if (i > 0)
            prereqs.push_back(events[i - 1]);

        auto event = Nanite::TaskGraph::Get().CreateAndDispatchTask(
            [i, &timestamp, &completionOrder]() {
                completionOrder[i].store(
                    timestamp.fetch_add(1, std::memory_order_acq_rel),
                    std::memory_order_release);
            },
            prereqs);

        events.push_back(event);
    }

    // 等待链尾任务完成
    events.back()->Wait();

    // 验证时间戳单调递增
    for (int i = 1; i < chainLength; ++i)
    {
        RC_ASSERT(completionOrder[i - 1].load(std::memory_order_acquire) <
                  completionOrder[i].load(std::memory_order_acquire));
    }

    Nanite::TaskGraph::Get().Shutdown();
}

// ============================================================
// 4.2  属性测试：任务完成事件状态（属性 2）
// Feature: nanite-testing, Property 2: 任务完成事件状态
// **Validates: Requirements 3.3**
//
// 对于任意数量的独立任务，全部完成后其 GraphEvent
// 的 IsComplete() 均应返回 true。
// ============================================================
RC_GTEST_PROP(TaskGraphPropertyTest, AllDispatchedTasksEventuallyComplete, ())
{
    // 生成任务数量 [1, 20]
    const auto taskCount = *rc::gen::inRange(1, 21);

    Nanite::TaskGraph::Get().Initialize(4);

    std::vector<Nanite::GraphEvent::Ptr> events;
    events.reserve(taskCount);

    for (int i = 0; i < taskCount; ++i)
    {
        auto event = Nanite::TaskGraph::Get().CreateAndDispatchTask(
            []() { /* 空任务体 */ });
        events.push_back(event);
    }

    // 等待所有任务完成
    Nanite::TaskGraph::WaitForEvents(events);

    // 验证所有事件标记为完成
    for (int i = 0; i < taskCount; ++i)
    {
        RC_ASSERT(events[i]->IsComplete());
    }

    Nanite::TaskGraph::Get().Shutdown();
}
