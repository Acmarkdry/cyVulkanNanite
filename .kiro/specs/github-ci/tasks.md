# 任务列表

## 任务 1：创建 CI 工作流文件

- [x] 1.1 创建 `.github/workflows/ci.yml`，定义工作流名称和触发条件（push/PR 到 main 分支）（需求 1.2, 1.3）
- [x] 1.2 定义构建矩阵：Windows（MSVC）、Linux（GCC）、Linux（Clang），Release 构建类型，`fail-fast: false`（需求 1.1, 1.4, 1.5）

## 任务 2：代码检出与子模块

- [x] 2.1 添加 `actions/checkout@v4` 步骤，启用 `submodules: recursive` 递归检出所有 Git 子模块（需求 2.4, 7.1）

## 任务 3：Vulkan SDK 安装

- [x] 3.1 添加 Linux Vulkan SDK 安装步骤，通过 LunarG APT 仓库安装（需求 2.1）
- [x] 3.2 添加 Windows Vulkan SDK 安装步骤，使用 `humbletim/install-vulkan-sdk` Action 或等效方式（需求 2.2）

## 任务 4：vcpkg 依赖管理与缓存

- [x] 4.1 添加 vcpkg 克隆和引导步骤（区分 Linux/Windows 的引导脚本）
- [x] 4.2 添加 `actions/cache@v4` 步骤，缓存 vcpkg 已安装的包，缓存键基于 OS、triplet 和工作流文件哈希（需求 3.1, 3.2, 3.4）
- [x] 4.3 添加 vcpkg install 步骤，安装 `openmesh` 和 `metis`，使用矩阵中定义的 triplet（需求 2.3）

## 任务 5：CMake 配置与构建

- [x] 5.1 添加 CMake 配置步骤，传递编译器、构建类型、vcpkg 工具链文件、triplet 和 `-DBUILD_TESTING=ON`（需求 4.1, 4.2, 4.3, 4.4）
- [x] 5.2 添加 CMake 构建步骤，使用 `--config` 参数支持多配置生成器（需求 4.1）

## 任务 6：测试执行

- [x] 6.1 添加 CTest 步骤，使用 `--verbose --output-on-failure --timeout 300` 参数运行测试（需求 5.1, 5.2, 5.4）

## 任务 7：构建产物上传

- [x] 7.1 添加 `actions/upload-artifact@v4` 步骤，上传 `build/bin/` 目录，产物名称包含矩阵变量，保留期 7 天（需求 6.1, 6.2, 6.3）

## 任务 8：删除旧工作流

- [x] 8.1 删除旧的模板工作流文件 `.github/workflows/cmake-multi-platform.yml`

## 任务 9：验证

- [x] 9.1 本地验证工作流 YAML 语法正确（可使用 `actionlint` 或在线验证工具）
- [x] 9.2 推送到分支并创建 PR，验证所有 3 个矩阵组合的 CI 运行结果

