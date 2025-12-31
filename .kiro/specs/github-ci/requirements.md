# 需求文档

## 简介

为 cyVulkanNanite 项目构建完善的 GitHub Actions CI 流水线，替换现有的模板化工作流。该 CI 系统需要支持多平台构建（Windows/Linux）、自动化测试、缓存优化、以及构建产物管理，确保每次代码提交和 PR 都能得到及时的质量反馈。

## 术语表

- **CI_Pipeline**: GitHub Actions 持续集成工作流，定义在 `.github/workflows/` 目录下的 YAML 文件
- **Build_Matrix**: CI 中的构建矩阵策略，定义操作系统、编译器、构建类型的组合
- **Dependency_Cache**: CI 中对第三方依赖（vcpkg 包、FetchContent 下载的 GTest/RapidCheck）的缓存机制
- **Build_Artifact**: CI 构建过程中产生的可执行文件和测试报告等输出产物
- **nanite_tests**: 项目的测试可执行目标，使用 GTest 和 RapidCheck 框架，通过 CTest 集成运行
- **vcpkg**: C++ 包管理器，项目通过 vcpkg 管理 OpenMesh 和 METIS 等依赖
- **FetchContent**: CMake 内置模块，项目用于在构建时下载 GTest 和 RapidCheck

## 需求

### 需求 1：多平台构建矩阵

**用户故事：** 作为开发者，我希望 CI 能在多个平台和编译器组合下构建项目，以便尽早发现平台相关的兼容性问题。

#### 验收标准

1. THE CI_Pipeline SHALL 定义包含 Windows（MSVC）和 Linux（GCC、Clang）的 Build_Matrix
2. WHEN 向 main 分支推送代码时，THE CI_Pipeline SHALL 触发所有 Build_Matrix 组合的构建
3. WHEN 向 main 分支提交 Pull Request 时，THE CI_Pipeline SHALL 触发所有 Build_Matrix 组合的构建
4. THE Build_Matrix SHALL 包含 Release 构建类型
5. WHEN Build_Matrix 中任一组合构建失败时，THE CI_Pipeline SHALL 将该组合标记为失败，同时继续执行其余组合的构建

### 需求 2：依赖管理与安装

**用户故事：** 作为开发者，我希望 CI 能自动安装项目所需的所有依赖，以便构建过程无需人工干预。

#### 验收标准

1. THE CI_Pipeline SHALL 在 Linux 环境中安装 Vulkan SDK
2. THE CI_Pipeline SHALL 在 Windows 环境中安装 Vulkan SDK
3. THE CI_Pipeline SHALL 通过 vcpkg 安装 OpenMesh 和 METIS 依赖
4. THE CI_Pipeline SHALL 在构建前检出所有 Git 子模块（glm、Vulkan-Assets）
5. WHEN 依赖安装失败时，THE CI_Pipeline SHALL 终止构建并报告具体的安装错误信息

### 需求 3：依赖缓存优化

**用户故事：** 作为开发者，我希望 CI 能缓存第三方依赖，以便缩短重复构建的等待时间。

#### 验收标准

1. THE CI_Pipeline SHALL 缓存 vcpkg 安装的包
2. THE CI_Pipeline SHALL 使用基于 vcpkg 配置文件内容的缓存键
3. WHEN 缓存命中时，THE CI_Pipeline SHALL 跳过依赖的重新下载和编译
4. WHEN vcpkg 配置文件内容发生变化时，THE CI_Pipeline SHALL 使缓存失效并重新安装依赖

### 需求 4：CMake 构建配置

**用户故事：** 作为开发者，我希望 CI 能正确配置和执行 CMake 构建，以便验证项目在干净环境下的可构建性。

#### 验收标准

1. THE CI_Pipeline SHALL 使用 CMake 配置项目，并传递 Build_Matrix 中指定的编译器和构建类型
2. THE CI_Pipeline SHALL 在 Windows 环境中通过 vcpkg 工具链文件配置 CMake
3. THE CI_Pipeline SHALL 在 Linux 环境中通过 vcpkg 工具链文件配置 CMake
4. THE CI_Pipeline SHALL 启用 BUILD_TESTING 选项以包含测试目标
5. WHEN CMake 配置步骤失败时，THE CI_Pipeline SHALL 终止构建并输出完整的 CMake 错误日志

### 需求 5：自动化测试执行

**用户故事：** 作为开发者，我希望 CI 能自动运行所有测试，以便每次提交都能验证代码的正确性。

#### 验收标准

1. WHEN 构建成功完成后，THE CI_Pipeline SHALL 通过 CTest 执行 nanite_tests 中的所有测试
2. THE CI_Pipeline SHALL 以详细模式运行 CTest 以输出每个测试用例的执行结果
3. WHEN 任一测试用例失败时，THE CI_Pipeline SHALL 将该构建组合标记为失败
4. IF CTest 执行超时，THEN THE CI_Pipeline SHALL 终止测试并报告超时错误
5. WHEN 测试执行完成后，THE CI_Pipeline SHALL 输出测试结果摘要，包含通过数和失败数

### 需求 6：构建产物管理

**用户故事：** 作为开发者，我希望 CI 能保存构建产物，以便在需要时下载和检查构建结果。

#### 验收标准

1. WHEN 构建成功完成后，THE CI_Pipeline SHALL 上传构建产生的可执行文件作为 Build_Artifact
2. THE CI_Pipeline SHALL 为每个 Build_Matrix 组合生成独立命名的 Build_Artifact
3. THE CI_Pipeline SHALL 设置 Build_Artifact 的保留期限为 7 天

### 需求 7：Git 子模块处理

**用户故事：** 作为开发者，我希望 CI 能正确处理 Git 子模块，以便构建过程能访问所有必要的外部依赖代码。

#### 验收标准

1. THE CI_Pipeline SHALL 在代码检出时递归初始化并更新所有 Git 子模块
2. WHEN 子模块检出失败时，THE CI_Pipeline SHALL 终止构建并报告子模块错误信息
