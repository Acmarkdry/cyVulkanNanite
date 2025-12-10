#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace Nanite
{
    // 前向声明
    class TaskGraph;

    // ========================================================================
    // GraphEvent — 任务完成事件，仿照 UE 的 FGraphEvent
    // 可作为其他任务的前置依赖（Prerequisites）
    // ========================================================================
    class GraphEvent
    {
    public:
        using Ptr = std::shared_ptr<GraphEvent>;

        static Ptr Create() { return std::make_shared<GraphEvent>(); }

        // 等待事件完成
        void Wait();

        // 标记事件完成，唤醒所有等待者，并触发后续任务
        void MarkComplete();

        bool IsComplete() const { return completed.load(std::memory_order_acquire); }

        // 添加后续任务：当本事件完成时，检查后续任务的依赖是否全部满足
        void AddSubsequent(class GraphTask* task);

    private:
        std::atomic<bool> completed{false};
        std::mutex mtx;
        std::condition_variable cv;
        std::vector<GraphTask*> subsequents;
    };

    // ========================================================================
    // GraphTask — 任务节点，仿照 UE 的 FBaseGraphTask
    // 持有前置依赖计数，当计数归零时自动提交到线程池执行
    // ========================================================================
    class GraphTask
    {
    public:
        using TaskFunction = std::function<void()>;

        // 构造任务，指定前置依赖
        GraphTask(TaskFunction func, const std::vector<GraphEvent::Ptr>& prerequisites, GraphEvent::Ptr completionEvent);

        // 前置依赖满足一个，减少计数；归零时提交执行
        void DecrementPrerequisites();

        // 获取完成事件
        GraphEvent::Ptr GetCompletionEvent() const { return completionEvent; }

    private:
        friend class TaskGraph;
        friend class GraphEvent;

        TaskFunction function;
        std::atomic<int> prerequisitesRemaining{0};
        GraphEvent::Ptr completionEvent;

        void Execute();
        void TryDispatch();
    };

    // ========================================================================
    // TaskGraph — 全局任务调度器 + 线程池，仿照 UE 的 FTaskGraphInterface
    // 单例模式，管理 worker 线程的生命周期
    // ========================================================================
    class TaskGraph
    {
    public:
        // 获取全局单例
        static TaskGraph& Get();

        // 初始化线程池（workerCount=0 表示使用硬件并发数）
        void Initialize(uint32_t workerCount = 0);

        // 关闭线程池，等待所有任务完成
        void Shutdown();

        // 提交一个就绪任务到线程池
        void Dispatch(GraphTask* task);

        // 创建并调度任务，返回完成事件
        // prerequisites: 前置依赖事件列表
        GraphEvent::Ptr CreateAndDispatchTask(
            GraphTask::TaskFunction func,
            const std::vector<GraphEvent::Ptr>& prerequisites = {});

        // 等待所有指定事件完成
        static void WaitForEvents(const std::vector<GraphEvent::Ptr>& events);

        uint32_t GetWorkerCount() const { return static_cast<uint32_t>(workers.size()); }

        ~TaskGraph();

    private:
        TaskGraph() = default;
        TaskGraph(const TaskGraph&) = delete;
        TaskGraph& operator=(const TaskGraph&) = delete;

        void WorkerLoop();

        std::vector<std::thread> workers;
        std::queue<GraphTask*> taskQueue;
        std::mutex queueMutex;
        std::condition_variable queueCV;
        std::atomic<bool> running{false};
        std::atomic<bool> initialized{false};
    };

    // ========================================================================
    // ParallelFor — 仿照 UE 的 ParallelFor
    // 将 [0, num) 的迭代拆分到多个任务并行执行
    // ========================================================================
    void ParallelFor(int32_t num, std::function<void(int32_t index)> body, bool forceSingleThread = false);

    // 带 minBatchSize 的版本：每个任务至少处理 minBatchSize 个元素
    void ParallelFor(int32_t num, int32_t minBatchSize, std::function<void(int32_t index)> body);

} // namespace Nanite
