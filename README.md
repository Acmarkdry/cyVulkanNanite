# cyVulkanNanite

基于 Vulkan 实现的 UE5 Nanite（虚拟几何体）系统，包含 GPU 驱动的 BVH 遍历、混合软硬光栅化、Hi-Z 遮挡剔除与 PBR/IBL 着色管线。

<!-- TODO: 补充效果截图 -->
<!-- ![效果展示](screenshots/demo.png) -->

## 核心特性

- **GPU 驱动的 BVH 遍历** — 层次包围盒遍历完全在 GPU 端通过 Compute Shader 完成
- **误差投影与 LOD 选择** — 基于屏幕空间误差度量的自动 LOD 切换，无需 CPU 回读
- **混合光栅化** — 大三角形走硬件光栅化（Geometry Shader），亚像素小三角形走软件光栅化（Compute Shader）
- **Hi-Z 遮挡剔除** — 层次 Z-Buffer 逐 mip 降采样，实现两趟遮挡剔除
- **视锥剔除** — GPU 端视锥体剔除，集成在 BVH 遍历阶段
- **Mesh 聚类与分组** — 基于 METIS 图分区的 Cluster 生成与 DAG 构建
- **延迟着色 + PBR/IBL** — G-Buffer 着色管线，支持基于图像的环境光照
- **Render Graph**（开发中）— 基于 Vulkan.hpp 的自动资源同步与布局转换
- **Indirect Draw/Dispatch** — 全 GPU 驱动绘制，每帧零 CPU-GPU 同步

## 技术栈

| 类别　　 | 技术　　　　　　　　　　　　　　　　　　　　|
| ----------| ---------------------------------------------|
| 图形 API | Vulkan 1.3　　　　　　　　　　　　　　　　　|
| 语言标准 | C++20　　　　　　　　　　　　　　　　　　　 |
| 着色器　 | GLSL 450+　　　　　　　　　　　　　　　　　 |
| 网格处理 | METIS 图分区、meshoptimizer 简化　　　　　　|
| 模型格式 | glTF 2.0　　　　　　　　　　　　　　　　　　|
| 构建系统 | CMake + vcpkg　　　　　　　　　　　　　　　 |
| 第三方库 | GLM, ImGui, tinygltf, KTX, stb, SPIRV-Cross |

## 渲染管线流程

```mermaid
graph TB
    A[场景数据] --> B[BVH遍历]
    B --> C[视锥剔除]
    C --> D{可见物体}
　　　　　　　　　　　 |

    D --> E[误差投影]
    E --> F[LOD选择]

    F --> G[硬件光栅化]
    F --> H[软件光栅化]

    G --> I[深度拷贝]
    H --> I
    I --> J[Hi-Z生成]

    J --> K[遮挡剔除]
    K --> L[合并光栅结果]

    L --> M[G-Buffer填充]
    M --> N[着色管线]
    N --> O[天空盒]
    O --> P[后处理]
    P --> Q[最终图像]

    style G fill:#90EE90
    style H fill:#FFB6C1
    style N fill:#87CEEB
```

## 项目结构

```
cyVulkanNanite/
├── base/                    # Vulkan 基础框架（设备、交换链、缓冲区、纹理等封装）
├── src/nanite/              # Nanite 核心算法
│   ├── Cluster.*            # Cluster 数据结构与构建
│   ├── ClusterGroup.*       # ClusterGroup + METIS 图分区
│   ├── NaniteMesh.*         # Nanite 网格管理（加载/序列化/反序列化）
│   ├── NaniteLodMesh.*      # LOD 处理（简化、BVH 构建、误差计算）
│   ├── NaniteInstance.*     # 实例化（世界变换、BVH 重建）
│   ├── NaniteScene.*        # 场景管理（多网格聚合、GPU Buffer 创建）
│   └── NaniteBVH.h          # BVH 节点定义
├── examples/pbrtexture/     # 主示例（PBR + Nanite 完整管线）
├── shaders/glsl/pbrtexture/ # GLSL 着色器源码
├── external/                # 第三方依赖
├── assets/                  # 模型与纹理资源
└── documents/               # 设计文档
```

## 构建

### 环境要求

- Windows 10/11
- Visual Studio 2022
- Vulkan SDK
- CMake 3.20+
- vcpkg

### 构建步骤

```bash
# 1. 拉取第三方依赖
vcpkg_windows.bat

# 2. 构建项目
build_windows.bat

# 或手动 CMake
mkdir build && cd build
cmake -D VCPKG_TARGET_TRIPLET=x64-windows-static ^
      -D CMAKE_TOOLCHAIN_FILE=../vcpkglib/vcpkg.windows/scripts/buildsystems/vcpkg.cmake ^
      -G "Visual Studio 17 2022" -A "x64" ..
msbuild cyVulkanNanite.sln /p:Configuration=Release
```

## 文档

- [TODO & Bug 追踪](documents/TODO-Fix-Bug.md)
- [Render Graph 设计思路](documents/RenderGraph-Design.md)
- [RenderPass 资源与同步梳理](documents/RenderPass-Sync.md)

## 参考资料

- [GAMES104 — GPU驱动的几何管线 Nanite](https://www.bilibili.com/video/BV1Et4y1P7ro/?share_source=copy_web&vd_source=de7a08b4d347de57ea41a8ae39a04d3b)
- [A Deep Dive into Nanite — SIGGRAPH 2021](https://advances.realtimerendering.com/s2021/Karis_Nanite_SIGGRAPH_Advances_2021_final.pdf)
- [Vulcanite — Mesh 处理参考](https://github.com/bdwhst/Vulcanite)
- [LegitEngine — Render Graph 参考](https://github.com/Raikiri/LegitEngine)

## 开源协议

本项目基于 [MIT License](LICENSE) 开源。
