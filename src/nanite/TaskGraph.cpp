#include "TaskGraph.h"
#include <algorithm>
#include <cassert>
#include <iostream>

namespace Nanite
{
    // ====================================================================
    // GraphEvent
    // ====================================================================

    void GraphEvent::Wait()
    {
        if (completed.load(std::memory_order_acquire))
            return;

        std::unique_lock lock(mtx);
        cv.wait(lock, [this] { return completed.load(std::memory_order_acquire); });
    }

    void GraphEvent::MarkComplete()
    {
        {
            std::lock_guard lock(mtx);
            completed.store(true, std::memory_order_release);

            // 通知所有后续任务：本依赖已完成
            for (auto* task : subsequents)
            {
                task->DecrementPrerequisites();
            }
            subsequents.clear();
        }
        cv.notify_all();
    }

    void GraphEvent::AddSubsequent(GraphTask* task)
    {
        std::lock_guard lock(mtx);
        // 如果事件已完成，直接减少后续任务的依赖计数
        if (completed.load(std::memory_order_acquire))
        {
            task->DecrementPrerequisites();
        }
        else
        {
            subsequents.push_back(task);
        }
    }

    // ====================================================================
    // GraphTask
    // ====================================================================

    GraphTask::GraphTask(TaskFunction func,
                         const std::vector<GraphEvent::Ptr>& prerequisites,
                         GraphEvent::Ptr completionEvent)
        : function(std::move(func))
        , prerequisitesRemaining(static_cast<int>(prerequisites.size()))
        , completionEvent(std::move(completionEvent))
    {
        if (prerequisites.empty())
        {
            // 无前置依赖，直接提交
            TryDispatch();
        }
        else
        {
            // 注册到每个前置事件的后续列表
            for (auto& prereq : prerequisites)
            {
                prereq->AddSubsequent(this);
            }
        }
    }

    void GraphTask::DecrementPrerequisites()
    {
        if (prerequisitesRemaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            // 所有前置依赖已满足，提交执行
            TryDispatch();
        }
    }

    void GraphTask::TryDispatch()
    {
        TaskGraph::Get().Dispatch(this);
    }

    void GraphTask::Execute()
    {
        function();
        if (completionEvent)
        {
            completionEvent->MarkComplete();
        }
    }

    // ====================================================================
    // TaskGraph（全局调度器 + 线程池）
    // ====================================================================

    TaskGraph& TaskGraph::Get()
    {
        static TaskGraph instance;
        return instance;
    }

    void TaskGraph::Initialize(uint32_t workerCount)
    {
        if (initialized.exchange(true))
            return; // 已初始化

        if (workerCount == 0)
            workerCount = std::max(1u, std::thread::hardware_concurrency());

        running.store(true, std::memory_order_release);

        workers.reserve(workerCount);
        for (uint32_t i = 0; i < workerCount; ++i)
        {
            workers.emplace_back(&TaskGraph::WorkerLoop, this);
        }

        std::cout << "[TaskGraph] Initialized with " << workerCount << " workers" << std::endl;
    }

    void TaskGraph::Shutdown()
    {
        if (!initialized.load())
            return;

        running.store(false, std::memory_order_release);
        queueCV.notify_all();

        for (auto& worker : workers)
        {
            if (worker.joinable())
                worker.join();
        }
        workers.clear();
        initialized.store(false);

        std::cout << "[TaskGraph] Shutdown complete" << std::endl;
    }

    TaskGraph::~TaskGraph()
    {
        Shutdown();
    }

    void TaskGraph::Dispatch(GraphTask* task)
    {
        {
            std::lock_guard lock(queueMutex);
            taskQueue.push(task);
        }
        queueCV.notify_one();
    }

    GraphEvent::Ptr TaskGraph::CreateAndDispatchTask(
        GraphTask::TaskFunction func,
        const std::vector<GraphEvent::Ptr>& prerequisites)
    {
        auto event = GraphEvent::Create();
        // GraphTask 在构造时会自动注册依赖或直接提交
        // 使用 new 分配，Execute 完成后由调度器管理生命周期
        new GraphTask(std::move(func), prerequisites, event);
        return event;
    }

    void TaskGraph::WaitForEvents(const std::vector<GraphEvent::Ptr>& events)
    {
        for (auto& event : events)
        {
            event->Wait();
        }
    }

    void TaskGraph::WorkerLoop()
    {
        while (true)
        {
            GraphTask* task = nullptr;
            {
                std::unique_lock lock(queueMutex);
                queueCV.wait(lock, [this] {
                    return !taskQueue.empty() || !running.load(std::memory_order_acquire);
                });

                if (!running.load(std::memory_order_acquire) && taskQueue.empty())
                    return;

                if (!taskQueue.empty())
                {
                    task = taskQueue.front();
                    taskQueue.pop();
                }
            }

            if (task)
            {
                task->Execute();
                delete task; // 任务执行完毕，释放内存
            }
        }
    }

    // ====================================================================
    // ParallelFor
    // ====================================================================

    void ParallelFor(int32_t num, std::function<void(int32_t)> body, bool forceSingleThread)
    {
        if (num <= 0) return;

        if (forceSingleThread || num == 1)
        {
            for (int32_t i = 0; i < num; ++i)
                body(i);
            return;
        }

        const auto workerCount = static_cast<int32_t>(TaskGraph::Get().GetWorkerCount());
        const int32_t batchCount = std::min(num, workerCount);
        const int32_t batchSize = (num + batchCount - 1) / batchCount;

        std::vector<GraphEvent::Ptr> events;
        events.reserve(batchCount);

        for (int32_t batch = 0; batch < batchCount; ++batch)
        {
            const int32_t begin = batch * batchSize;
            const int32_t end = std::min(begin + batchSize, num);

            events.push_back(TaskGraph::Get().CreateAndDispatchTask(
                [begin, end, &body]()
                {
                    for (int32_t i = begin; i < end; ++i)
                        body(i);
                }));
        }

        TaskGraph::WaitForEvents(events);
    }

    void ParallelFor(int32_t num, int32_t minBatchSize, std::function<void(int32_t)> body)
    {
        if (num <= 0) return;

        const auto workerCount = static_cast<int32_t>(TaskGraph::Get().GetWorkerCount());
        const int32_t maxBatches = std::max(1, num / std::max(1, minBatchSize));
        const int32_t batchCount = std::min(maxBatches, workerCount);

        if (batchCount <= 1)
        {
            for (int32_t i = 0; i < num; ++i)
                body(i);
            return;
        }

        const int32_t batchSize = (num + batchCount - 1) / batchCount;

        std::vector<GraphEvent::Ptr> events;
        events.reserve(batchCount);

        for (int32_t batch = 0; batch < batchCount; ++batch)
        {
            const int32_t begin = batch * batchSize;
            const int32_t end = std::min(begin + batchSize, num);

            events.push_back(TaskGraph::Get().CreateAndDispatchTask(
                [begin, end, &body]()
                {
                    for (int32_t i = begin; i < end; ++i)
                        body(i);
                }));
        }

        TaskGraph::WaitForEvents(events);
    }

} // namespace Nanite
