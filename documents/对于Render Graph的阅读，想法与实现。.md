读了一下[这个项目](https://github.com/Raikiri/LegitEngine)的代码，感觉对于一个复杂工程的实现大家好像都差不多（主要是这个代码的实现思路太像ue的levelsequence了）：core负责持有关键资源方便获取，然后一个模板attribute，加一点特化，再加一点资源持有。
## Legit Engine的缺点
+ 没有DAG分析，也就意味着无法进行re sort优化。
+ 没有异步队列，单队列执行。
## 核心代码
Synchronization -> vulkan的同步模型相关
```C++
namespace legit {
    // ═══════════════ 1. 队列族类型 ═══════════════
    enum struct QueueFamilyTypes { Graphics, Transfer, Compute, Present, Undefined };
    
    // ═══════════════ 2. 图像访问模式 ═══════════════
    struct ImageAccessPattern { stage, accessMask, layout, queueFamilyType };
    
    // ═══════════════ 3. 图像使用类型枚举 ═══════════════
    enum struct ImageUsageTypes { ... };
    
    // ═══════════════ 4. 图像访问模式转换函数 ═══════════════
    static ImageAccessPattern GetSrcImageAccessPattern(ImageUsageTypes);
    static ImageAccessPattern GetDstImageAccessPattern(ImageUsageTypes);
    static bool IsImageBarrierNeeded(ImageUsageTypes src, ImageUsageTypes dst);
    
    // ═══════════════ 5. 缓冲访问模式（类似图像）═══════════════
    struct BufferAccessPattern { ... };
    enum struct BufferUsageTypes { ... };
    static BufferAccessPattern GetSrcBufferAccessPattern(BufferUsageTypes);
    static BufferAccessPattern GetDstBufferAccessPattern(BufferUsageTypes);
}
```
其中scr只关心write，dst只关心read+write。
RenderGraph -> 前面一部分是resource proxy，中间render context，execute执行核心逻辑。
execute阶段可以拆分为：
```plain
Execute()
    │
    ├─→ ResolveImages()         // 为每个 ImageProxy 分配真实图像
    ├─→ ResolveImageViews()     // 创建对应的 ImageView
    ├─→ ResolveBuffers()        // 为每个 BufferProxy 分配缓冲
    │
    └─→ for (task : tasks):
            │
            ├─→ 查询资源的上一次使用类型
            │   └─→ GetLastImageSubresourceUsageType()
            │
            ├─→ 生成布局转换屏障
            │   └─→ AddImageTransitionBarriers()
            │
            ├─→ 提交屏障
            │   └─→ commandBuffer.pipelineBarrier()
            │
            ├─→ 开始 RenderPass (如果是渲染任务)
            │   └─→ framebufferCache.BeginPass()
            │
            ├─→ 执行用户代码
            │   └─→ desc.recordFunc(passContext)
            │
            └─→ 结束 RenderPass
                └─→ framebufferCache.EndPass()
```

ssvgirenderer -> 一个实际使用实例。
render pass cache -> render pass缓存。
一些对于vulkan handle的再封装。
pool -> Proxy ID池子管理，为每个 virtual
resource分配一个id。因为声明pass的时候资源还不存在，所以用proxy id作为一个占位符，execute阶段再从缓存池分配真实资源，从而填充resolve image
handles -> unique handle raii封装
shader memory pool -> uniform数据管理
## 关于render graph的职责
查看ssvgirenderer的例子有一个问题，render graph的职责界限在哪里？
```plain
┌─────────────────────────────────────────────────────────────────────────────┐
│                    RenderGraph 职责边界                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   RenderGraph 自动处理 ✅                    用户必须手动处理 ❌             │
│   ─────────────────────                     ────────────────────            │
│                                                                             │
│   • 瞬态资源分配/复用                        • Pipeline 创建/绑定           │
│   • Image Layout 转换                        • Descriptor Set 创建/绑定    │
│   • Pipeline Barrier 插入                    • Uniform 数据上传            │
│   • RenderPass 创建/缓存                     • 顶点/索引缓冲绑定            │
│   • Framebuffer 创建/缓存                    • Draw Call 命令              │
│                                                                             │
│   "我帮你管好资源状态"                        "具体画什么你自己决定"         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```
所以如果转换为使用material系统，就可以很好的提高代码复用率。
