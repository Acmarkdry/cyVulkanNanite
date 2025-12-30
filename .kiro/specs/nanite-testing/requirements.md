# 需求文档

## 简介

为 cyVulkanNanite 项目引入测试框架，针对 Task Graph（任务图调度系统）和 Nanite 几何处理模块编写单元测试与集成测试。测试框架需要与现有 CMake 构建系统集成，不依赖 Vulkan 运行时环境，能够独立编译和运行。

## 术语表

- **TaskGraph**: 全局任务调度器与线程池，管理 worker 线程的生命周期，提供任务创建、依赖管理和并行执行功能
- **GraphEvent**: 任务完成事件，可作为其他任务的前置依赖，支持等待和完成通知
- **GraphTask**: 任务节点，持有前置依赖计数，当计数归零时自动提交到线程池执行
- **ParallelFor**: 将 [0, num) 的迭代拆分到多个任务并行执行的工具函数
- **Cluster**: Nanite 几何处理中的三角形簇，包含三角形索引、包围球、LOD 误差等信息
- **ClusterNode**: 展平后的 Cluster 节点，存储 LOD 误差和包围球信息
- **ClusterGroup**: 多个 Cluster 的分组，使用 METIS 图分区算法进行局部三角形聚类
- **Graph**: 邻接图数据结构，用于表示三角形或 Cluster 之间的连接关系
- **MetisGraph**: METIS 库所需的 CSR 格式图数据结构
- **NaniteLodMesh**: 单个 LOD 层级的网格数据，包含聚类、分组、简化和 BVH 构建功能
- **NaniteBVH**: Nanite 的层次包围体（Bounding Volume Hierarchy），用于加速空间查询
- **TestRunner**: 测试可执行程序，独立于主渲染程序编译和运行
- **JSON序列化**: 使用 nlohmann::json 库将 Cluster 和 BVH 节点数据序列化为 JSON 格式

## 需求

### 需求 1：测试框架集成

**用户故事：** 作为开发者，我希望项目中集成一个 C++ 测试框架，以便能够编写和运行自动化测试。

#### 验收标准

1. THE TestRunner SHALL 使用 Google Test 框架编译为独立的可执行程序
2. THE TestRunner SHALL 通过 CMake 的 `add_subdirectory` 集成到现有构建系统中
3. THE TestRunner SHALL 链接 `nanite_core` 静态库以访问被测代码
4. THE TestRunner SHALL 在不依赖 Vulkan 运行时和 GPU 设备的情况下编译和运行
5. WHEN 执行 `ctest` 或直接运行测试可执行程序时，THE TestRunner SHALL 输出每个测试用例的通过或失败结果

### 需求 2：TaskGraph 线程池生命周期测试

**用户故事：** 作为开发者，我希望验证 TaskGraph 线程池的初始化和关闭行为正确，以确保资源管理无泄漏。

#### 验收标准

1. WHEN 调用 `TaskGraph::Initialize()` 时，THE TaskGraph SHALL 创建指定数量的 worker 线程
2. WHEN 调用 `TaskGraph::Initialize(0)` 时，THE TaskGraph SHALL 使用 `std::thread::hardware_concurrency()` 作为 worker 数量
3. WHEN 调用 `TaskGraph::Shutdown()` 时，THE TaskGraph SHALL 等待所有 worker 线程结束并释放资源
4. WHEN 重复调用 `TaskGraph::Initialize()` 时，THE TaskGraph SHALL 仅执行一次初始化，忽略后续调用
5. WHEN `TaskGraph::Shutdown()` 在未初始化时被调用，THE TaskGraph SHALL 安全返回而不产生错误

### 需求 3：任务创建与依赖调度测试

**用户故事：** 作为开发者，我希望验证任务的创建、依赖管理和执行顺序正确，以确保并发调度的可靠性。

#### 验收标准

1. WHEN 调用 `CreateAndDispatchTask` 且无前置依赖时，THE TaskGraph SHALL 立即将任务提交到线程池执行
2. WHEN 调用 `CreateAndDispatchTask` 且存在前置依赖时，THE GraphTask SHALL 在所有前置依赖完成后才执行
3. WHEN 任务执行完成时，THE GraphEvent SHALL 被标记为完成状态，且 `IsComplete()` 返回 true
4. WHEN 多个任务依赖同一个 GraphEvent 时，THE GraphEvent SHALL 在完成时通知所有后续任务
5. WHEN 调用 `GraphEvent::Wait()` 且事件已完成时，THE GraphEvent SHALL 立即返回而不阻塞
6. WHEN 构建任务依赖链 A → B → C 时，THE TaskGraph SHALL 保证 A 在 B 之前完成，B 在 C 之前完成

### 需求 4：ParallelFor 正确性测试

**用户故事：** 作为开发者，我希望验证 ParallelFor 能正确地并行处理所有元素，以确保数据完整性。

#### 验收标准

1. WHEN 调用 `ParallelFor(num, body)` 时，THE ParallelFor SHALL 对 [0, num) 范围内的每个索引恰好执行一次 body 函数
2. WHEN `num <= 0` 时，THE ParallelFor SHALL 不执行任何操作并安全返回
3. WHEN `forceSingleThread` 为 true 时，THE ParallelFor SHALL 在调用线程上顺序执行所有迭代
4. WHEN 调用带 `minBatchSize` 的 `ParallelFor` 时，THE ParallelFor SHALL 确保每个批次至少处理 `minBatchSize` 个元素
5. FOR ALL 有效的 num 值，ParallelFor 的并行执行结果 SHALL 与顺序执行结果一致（幂等性）

### 需求 5：Graph 与 MetisGraph 数据结构测试

**用户故事：** 作为开发者，我希望验证图数据结构的构建和转换逻辑正确，以确保 METIS 分区的输入数据有效。

#### 验收标准

1. WHEN 调用 `Graph::addEdge(from, to, cost)` 时，THE Graph SHALL 在邻接表中记录从 from 到 to 的边及其权重
2. WHEN 调用 `Graph::addEdgeCost(from, to, cost)` 时，THE Graph SHALL 将 cost 累加到已有边的权重上
3. WHEN 调用 `MetisGraph::GraphToMetisGraph(graph)` 时，THE MetisGraph SHALL 生成有效的 CSR 格式数据
4. FOR ALL 转换后的 MetisGraph，`xadj` 数组的长度 SHALL 等于顶点数加一
5. FOR ALL 转换后的 MetisGraph，`adjncy` 和 `adjwgt` 数组的长度 SHALL 相等

### 需求 6：Cluster JSON 序列化往返测试

**用户故事：** 作为开发者，我希望验证 Cluster 和 ClusterNode 的 JSON 序列化与反序列化能正确往返，以确保缓存数据的完整性。

#### 验收标准

1. FOR ALL 有效的 Cluster 对象，执行 `toJson()` 后再 `fromJson()` SHALL 产生与原始对象等价的 Cluster
2. FOR ALL 有效的 ClusterNode 对象，执行 `toJson()` 后再 `fromJson()` SHALL 产生与原始对象等价的 ClusterNode
3. WHEN `fromJson()` 接收到缺少必要字段的 JSON 数据时，THE Cluster SHALL 触发断言失败
4. THE Cluster 的 `toJson()` 输出 SHALL 包含 `normalizedlodError`、`parentNormalizedError`、`lodError`、`boundingSphereCenter`、`boundingSphereRadius`、`parentClusterIndices` 和 `triangleIndices` 字段

### 需求 7：NaniteBVHNodeInfo JSON 序列化往返测试

**用户故事：** 作为开发者，我希望验证 BVH 节点信息的序列化与反序列化能正确往返，以确保 BVH 缓存数据的完整性。

#### 验收标准

1. FOR ALL 有效的 NaniteBVHNodeInfo 对象，执行 `toJson()` 后再 `fromJson()` SHALL 产生与原始对象等价的 NaniteBVHNodeInfo
2. WHEN `fromJson()` 接收到缺少必要字段的 JSON 数据时，THE NaniteBVHNodeInfo SHALL 触发断言失败
3. THE NaniteBVHNodeInfo 的序列化 SHALL 正确保存和恢复所有 `CLUSTER_GROUP_MAX_SIZE` 个 clusterIndices 元素

### 需求 8：NaniteLodMesh JSON 序列化往返测试

**用户故事：** 作为开发者，我希望验证 NaniteLodMesh 的 JSON 序列化与反序列化能正确往返，以确保 LOD 网格缓存数据的完整性。

#### 验收标准

1. FOR ALL 有效的 NaniteLodMesh 对象，执行 `toJson()` 后再 `fromJson()` SHALL 产生与原始对象等价的 NaniteLodMesh
2. THE NaniteLodMesh 的序列化 SHALL 正确保存和恢复 `clusterNum`、`triangleClusterIndex`、`clusterColorAssignment`、`clusters` 等字段
3. FOR ALL 序列化后的 NaniteLodMesh，反序列化后的 clusters 数量 SHALL 等于 `clusterNum`

### 需求 9：几何工具函数测试

**用户故事：** 作为开发者，我希望验证几何计算工具函数的正确性，以确保包围盒和包围球计算结果准确。

#### 验收标准

1. WHEN 调用 `getTriangleAABB(p0, p1, p2, pMin, pMax)` 时，THE 工具函数 SHALL 计算出包含三个顶点的最小轴对齐包围盒
2. FOR ALL 三角形顶点 p0、p1、p2，计算出的 pMin 的每个分量 SHALL 小于等于三个顶点对应分量的最小值
3. FOR ALL 三角形顶点 p0、p1、p2，计算出的 pMax 的每个分量 SHALL 大于等于三个顶点对应分量的最大值
4. WHEN 三个顶点相同时，THE 工具函数 SHALL 返回退化的包围盒（pMin 等于 pMax 等于该顶点）

### 需求 10：ClusterInfo AABB 合并测试

**用户故事：** 作为开发者，我希望验证 ClusterInfo 的 AABB 合并逻辑正确，以确保空间查询的准确性。

#### 验收标准

1. WHEN 调用 `ClusterInfo::mergeAABB(pMinOther, pMaxOther)` 时，THE ClusterInfo SHALL 将 pMinWorld 更新为两个包围盒 pMin 的分量最小值
2. WHEN 调用 `ClusterInfo::mergeAABB(pMinOther, pMaxOther)` 时，THE ClusterInfo SHALL 将 pMaxWorld 更新为两个包围盒 pMax 的分量最大值
3. FOR ALL 合并操作，合并后的包围盒 SHALL 完全包含合并前的两个包围盒（单调性）
4. WHEN 对同一个 ClusterInfo 连续执行多次 `mergeAABB` 时，THE ClusterInfo 的包围盒 SHALL 包含所有已合并的包围盒
