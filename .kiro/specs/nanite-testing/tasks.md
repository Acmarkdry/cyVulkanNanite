# 任务列表

## 任务 1：CMake 构建系统集成

- [x] 1.1 创建 `tests/CMakeLists.txt`，使用 FetchContent 引入 Google Test 和 RapidCheck，创建 `nanite_tests` 可执行目标，链接 `nanite_core`、`GTest::gtest_main`、`rapidcheck`，注册 `gtest_discover_tests`
- [x] 1.2 在根 `CMakeLists.txt` 中添加 `add_subdirectory(tests)`
- [x] 1.3 创建 `tests/main.cpp` 作为测试入口（使用 `gtest_main` 则可为空占位）
- [x] 1.4 验证 `nanite_tests` 可在不依赖 Vulkan 运行时的情况下编译通过

## 任务 2：RapidCheck 自定义生成器

- [x] 2.1 创建 `tests/generators.h`，实现 `glm::vec3` 和 `glm::vec4` 的 `rc::Arbitrary` 特化（浮点范围 [-1e6, 1e6]）
- [x] 2.2 实现 `Nanite::Cluster` 的 RapidCheck 生成器（生成有效的序列化字段）
- [x] 2.3 实现 `Nanite::ClusterNode` 的 RapidCheck 生成器
- [x] 2.4 实现 `Nanite::NaniteBVHNodeInfo` 的 RapidCheck 生成器（包含有效枚举值和 clusterIndices 数组）
- [x] 2.5 实现 `Nanite::NaniteLodMesh` 的 RapidCheck 生成器（仅序列化相关字段，clusters 数量与 clusterNum 一致）

## 任务 3：TaskGraph 单元测试

- [x] 3.1 创建 `tests/test_taskgraph.cpp`，编写 TaskGraph 初始化测试：验证 Initialize(N) 后 GetWorkerCount() == N（需求 2.1）
- [x] 3.2 编写 Initialize(0) 使用 hardware_concurrency 的测试（需求 2.2）
- [x] 3.3 编写 Shutdown 后资源释放的测试（需求 2.3）
- [x] 3.4 编写重复 Initialize 幂等性测试（需求 2.4）
- [x] 3.5 编写未初始化时 Shutdown 安全返回的测试（需求 2.5）
- [x] 3.6 编写无依赖任务立即执行的测试（需求 3.1）
- [x] 3.7 编写多任务依赖同一事件的扇出测试（需求 3.4）
- [x] 3.8 编写已完成事件 Wait 立即返回的测试（需求 3.5）
- [x] 3.9 编写具体依赖链 A→B→C 执行顺序的测试（需求 3.6）

## 任务 4：TaskGraph 属性测试

- [x] 4.1 在 `tests/test_taskgraph.cpp` 中编写属性测试：任务依赖执行顺序（属性 1）——生成随机长度的依赖链，验证时间戳单调递增
- [x] 4.2 编写属性测试：任务完成事件状态（属性 2）——生成随机数量的任务，验证所有事件最终标记为完成

## 任务 5：ParallelFor 测试

- [x] 5.1 创建 `tests/test_parallelfor.cpp`，编写 ParallelFor num<=0 安全返回的单元测试（需求 4.2）
- [x] 5.2 编写 forceSingleThread 模式的单元测试（需求 4.3）
- [x] 5.3 编写属性测试：ParallelFor 并行与顺序结果一致（属性 3）——生成随机 num，用原子数组验证每个索引恰好执行一次

## 任务 6：Graph 与 MetisGraph 测试

- [x] 6.1 创建 `tests/test_graph.cpp`，编写属性测试：Graph::addEdge 记录边（属性 4）
- [x] 6.2 编写属性测试：Graph::addEdgeCost 累加权重（属性 5）
- [x] 6.3 编写属性测试：GraphToMetisGraph CSR 格式有效性（属性 6）——生成随机图，验证 xadj 长度、adjncy/adjwgt 长度相等、邻居数量一致

## 任务 7：Cluster JSON 序列化测试

- [x] 7.1 创建 `tests/test_cluster_json.cpp`，编写属性测试：Cluster JSON 序列化往返（属性 7）
- [x] 7.2 编写属性测试：ClusterNode JSON 序列化往返（属性 8）
- [x] 7.3 编写单元测试：Cluster::fromJson 缺少字段时触发断言（需求 6.3，使用 EXPECT_DEATH）

## 任务 8：NaniteBVHNodeInfo JSON 序列化测试

- [x] 8.1 创建 `tests/test_bvh_json.cpp`，编写属性测试：NaniteBVHNodeInfo JSON 序列化往返（属性 9）
- [x] 8.2 编写单元测试：NaniteBVHNodeInfo::fromJson 缺少字段时触发断言（需求 7.2，使用 EXPECT_DEATH）

## 任务 9：NaniteLodMesh JSON 序列化测试

- [x] 9.1 创建 `tests/test_lodmesh_json.cpp`，编写属性测试：NaniteLodMesh JSON 序列化往返（属性 10）

## 任务 10：几何工具函数测试

- [x] 10.1 创建 `tests/test_geometry.cpp`，编写属性测试：getTriangleAABB 计算正确性（属性 11）
- [x] 10.2 编写单元测试：退化三角形（三个顶点相同）的 AABB 计算（需求 9.4）

## 任务 11：ClusterInfo AABB 合并测试

- [x] 11.1 创建 `tests/test_clusterinfo.cpp`，编写属性测试：ClusterInfo::mergeAABB 单调包含性（属性 12）——生成随机序列的 AABB，逐个合并后验证最终包围盒包含所有输入
