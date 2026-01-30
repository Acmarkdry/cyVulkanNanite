---
inclusion: fileMatch
fileMatchPattern: "**/RenderGraph.h"
---

# RenderGraph 架构分析

## 整体设计

RenderGraph 是一个 Vulkan render graph 实现，核心价值在于自动化 Vulkan 最繁琐的同步和资源管理。用户只需声明每个 pass 使用哪些资源，graph 自动处理布局转换、barrier 插入、render pass/framebuffer 缓存等。

## 资源代理系统（Proxy System）

- `ImageProxy` / `ImageViewProxy` / `BufferProxy` 是资源的间接引用
- 分为 `Transient`（graph 内部管理）和 `External`（外部传入）两种类型
- `RenderGraphProxyId` 默认构造 id 为 `size_t(-1)` 表示无效，只重载了 `operator==` 没有 `operator!=`，所以代码中用 `!(a == b)` 判断不等

## Execute 方法 — 六种 Task 类型

### RenderPass
完整的光栅化管线执行流程：
1. 解析 proxy → 真实资源，填入 `RenderPassContext`
2. 为所有资源插入 pipeline barrier（image 转到对应布局，buffer 设置正确的 access mask）
3. 通过 `RenderPassCache` 和 `FramebufferCache` 获取/创建缓存的 Vulkan 对象
4. `BeginPass` → 用户 `recordFunc` 回调 → `EndPass`

资源类型及目标用途：
- `inputImageViewProxies` → `GraphicsShaderRead`
- `inoutStorageImageProxies` → `GraphicsShaderReadWrite`
- `colorAttachments` → `ColorAttachment`
- `depthAttachment` → `DepthAttachment`
- `vertexBufferProxies` → `VertexBuffer`
- `inoutStorageBufferProxies` → `GraphicsShaderReadWrite`

### ComputePass
- 使用 `PassContext`（非 `RenderPassContext`），不需要 render pass / framebuffer
- 资源用途为 `ComputeShaderRead` / `ComputeShaderReadWrite`（pipeline stage 是 `eComputeShader`）
- `recordFunc` 有空检查（允许只插 barrier 不 dispatch）
- 无 `BeginPass` / `EndPass`

### TransferPass
- 资源按传输方向分为 `src`（TransferSrc）和 `dst`（TransferDst）
- pipeline stage 为 `eTransfer`
- `recordFunc` 有空检查，无 `BeginPass` / `EndPass`

### ImagePresent
- 仅将 swapchain image 布局转换到 `Present`
- 无 PassContext，无 recordFunc（present 通过 `vkQueuePresentKHR` 完成，不走 command buffer）

### FrameSyncBegin
- 全局 memory barrier：`eBottomOfPipe` → `eTopOfPipe`
- 确保上一帧所有 GPU 操作完成后再开始新帧
- `FrameSyncBeginPassDesc` 是空结构体，仅作为 task 队列中的占位

### FrameSyncEnd
- 遍历所有外部 image view，将布局恢复到 `externalUsageType`
- 过滤条件：`externalView != nullptr` 且用途不是 `Unknown` / `None`
- 外部 buffer 恢复逻辑被注释掉，尚未实现

## Barrier 自动推导链

完整调用链：

```
Execute 的每个 case
  → AddImageTransitionBarriers / AddBufferBarriers
    → GetLastXxxUsageType（回溯查上一次用途）
      → GetTaskXxxUsageType（查某个 task 对资源的用途）
        → ImageViewContainsSubresource（判断 subresource 是否在 view 范围内）
    → FlushXxxTransitionBarriers（生成 Vulkan barrier 对象）
      → 累积到 barriers 列表
  → commandBuffer.pipelineBarrier（一次性提交所有 barrier）
```

### ImageViewContainsSubresource
判断某个具体的 mip level + array layer 是否落在某个 image view 的范围内。三个条件：
1. 同一张底层图像
2. array layer 在 view 范围内
3. mip level 在 view 范围内

### GetTaskImageSubresourceUsageType / GetTaskBufferUsageType
给定一个 task 和一个资源，返回该 task 对该资源的用途类型。按 task 类型 switch，遍历该 task 的所有资源引用逐个匹配。
- Image 版本精确到 mip+layer 级别（通过 `ImageViewContainsSubresource`）
- Buffer 版本直接比较 handle

### GetLastImageSubresourceUsageType / GetLastBufferUsageType
从当前 task 往前回溯，找到资源上一次被使用时的用途。这就是 barrier 的"源状态"。
- Image 版本有两层 fallback：先查 task 历史，再查外部图像的 `externalUsageType`
- Buffer 版本没有外部 fallback，直接返回 `None`

### FlushImageTransitionBarriers / FlushBufferTransitionBarriers
barrier 生成的最底层。将抽象的 `UsageType` 映射为具体的 Vulkan access mask + layout + pipeline stage，构造 `ImageMemoryBarrier` 或 `BufferMemoryBarrier`。
- `srcStage` / `dstStage` 用 `|=` 累加（一次 pipelineBarrier 提交多个 barrier 时需要并集）
- Queue family ownership transfer 代码存在但未启用（两个分支都设为 `VK_QUEUE_FAMILY_IGNORED`）
- Buffer barrier 没有 layout 概念，用 `offset=0, size=VK_WHOLE_SIZE` 覆盖整个 buffer

## 已知问题

1. `GetTaskBufferUsageType` 中 TransferPass 的 dst 分支错误返回 `TransferSrc`，应为 `TransferDst`
2. `GetLastBufferUsageType` 循环从 `taskOffset = 1` 开始，会漏掉对 task 0 的检查（image 版本从 `taskOffset = 0` 开始，是正确的）
3. `ImagePresent` case 中变量名 typo：`imagePesentDesc` 少了个 `r`
4. `FlushExternalImages` 方法体被注释掉，功能由 `FrameSyncEnd` 替代
5. 外部 buffer 的状态恢复（`FrameSyncEnd` 中）被注释掉，尚未实现
6. Queue family ownership transfer 未实现，所有 barrier 都用 `VK_QUEUE_FAMILY_IGNORED`
