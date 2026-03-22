#include "TaskGraph.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>

namespace Nanite
{
    // ====================================================================
    // 全局 METIS 互斥锁
    // ====================================================================

    std::mutex& GetMetisMutex()
    {
        static std::mutex metisMutex;
        return metisMutex;
    }
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

    GraphTask::GraphTask(TaskFunction func, GraphEvent::Ptr completionEvent)
        : function(std::move(func))
        , completionEvent(std::move(completionEvent))
    {
    }

    void GraphTask::CreateAndSetup(TaskFunction func,
                         const std::vector<GraphEvent::Ptr>& prerequisites,
                         GraphEvent::Ptr completionEvent)
    {
        auto task = std::make_unique<GraphTask>(std::move(func), std::move(completionEvent));
        task->prerequisitesRemaining.store(static_cast<int>(prerequisites.size()), std::memory_order_relaxed);

        if (prerequisites.empty())
        {
            // 无前置依赖，直接提交（所有权转移给线程池）
            task->TryDispatch();
            // TryDispatch 内部会 move 走 unique_ptr，此处 task 已为空
        }
        else
        {
            // 先获取裸指针用于注册依赖，所有权暂由 GraphEvent::AddSubsequent 管理
            // 注意：最后一个 DecrementPrerequisites 会触发 TryDispatch，将所有权转移给线程池
            auto* rawTask = task.release();
            for (auto& prereq : prerequisites)
            {
                prereq->AddSubsequent(rawTask);
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
        // 将自身包装为 unique_ptr 转移给线程池
        TaskGraph::Get().Dispatch(std::unique_ptr<GraphTask>(this));
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

        // 清理队列中未执行的残留任务，防止内存泄漏
        DrainRemainingTasks();

        initialized.store(false);

        std::cout << "[TaskGraph] Shutdown complete" << std::endl;
    }

    TaskGraph::~TaskGraph()
    {
        Shutdown();
    }

    void TaskGraph::Dispatch(std::unique_ptr<GraphTask> task)
    {
        {
            std::lock_guard lock(queueMutex);
            taskQueue.push(std::move(task));
        }
        queueCV.notify_one();
    }

    GraphEvent::Ptr TaskGraph::CreateAndDispatchTask(
        GraphTask::TaskFunction func,
        const std::vector<GraphEvent::Ptr>& prerequisites)
    {
        auto event = GraphEvent::Create();
        GraphTask::CreateAndSetup(std::move(func), prerequisites, event);
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
            std::unique_ptr<GraphTask> task;
            {
                std::unique_lock lock(queueMutex);
                queueCV.wait(lock, [this] {
                    return !taskQueue.empty() || !running.load(std::memory_order_acquire);
                });

                if (!running.load(std::memory_order_acquire) && taskQueue.empty())
                    return;

                if (!taskQueue.empty())
                {
                    task = std::move(taskQueue.front());
                    taskQueue.pop();
                }
            }

            if (task)
            {
                task->Execute();
                // unique_ptr 自动释放 GraphTask
            }
        }
    }

    void TaskGraph::DrainRemainingTasks()
    {
        std::lock_guard lock(queueMutex);
        while (!taskQueue.empty())
        {
            taskQueue.pop(); // unique_ptr 析构自动释放
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

        // 用 shared_ptr 包装 body，避免按引用捕获局部变量的生命周期风险
        auto sharedBody = std::make_shared<std::function<void(int32_t)>>(std::move(body));

        std::vector<GraphEvent::Ptr> events;
        events.reserve(batchCount);

        for (int32_t batch = 0; batch < batchCount; ++batch)
        {
            const int32_t begin = batch * batchSize;
            const int32_t end = std::min(begin + batchSize, num);

            events.push_back(TaskGraph::Get().CreateAndDispatchTask(
                [begin, end, sharedBody]()
                {
                    for (int32_t i = begin; i < end; ++i)
                        (*sharedBody)(i);
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

        auto sharedBody = std::make_shared<std::function<void(int32_t)>>(std::move(body));

        std::vector<GraphEvent::Ptr> events;
        events.reserve(batchCount);

        for (int32_t batch = 0; batch < batchCount; ++batch)
        {
            const int32_t begin = batch * batchSize;
            const int32_t end = std::min(begin + batchSize, num);

            events.push_back(TaskGraph::Get().CreateAndDispatchTask(
                [begin, end, sharedBody]()
                {
                    for (int32_t i = begin; i < end; ++i)
                        (*sharedBody)(i);
                }));
        }

        TaskGraph::WaitForEvents(events);
    }

} // namespace Nanite
