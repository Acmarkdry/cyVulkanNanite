---
inclusion: always
---

# cyVulkanNanite — AI Coding Rules & Guidelines

## 1. High-Level Architecture Overview

### 1.1 Project Overview
This project is a **Vulkan-based Nanite (UE5 Virtual Geometry) implementation** with core features:
- GPU-driven BVH traversal and LOD selection
- Error Projection-based hierarchical detail
- Frustum Culling + Hi-Z Occlusion Culling
- Hardware + Software rasterization hybrid pipeline
- PBR + IBL shading pipeline

### 1.2 Directory Structure
```
cyVulkanNanite/
├── base/                    # Vulkan base framework (Sascha Willems style)
│   ├── vulkanexamplebase.*  # Example base class, manages window, swapchain, main loop
│   ├── VulkanDevice.*       # Physical/logical device wrapper
│   ├── VulkanBuffer.*       # GPU Buffer wrapper
│   ├── VulkanTexture.*      # Texture2D / CubeMap / Array wrapper
│   ├── VulkanInitializers.hpp  # Vulkan struct initialization factory functions (inline)
│   ├── VulkanDescriptorManager.*  # Descriptor set management (Singleton)
│   ├── VulkanUIOverlay.*    # ImGui overlay
│   ├── VulkanSwapChain.*    # Swapchain wrapper
│   ├── VulkanglTFModel.*    # glTF model loading
│   ├── VulkanTools.*        # Utility functions + VK_CHECK_RESULT macro
│   ├── VulkanDebug.*        # Vulkan debug utilities
│   ├── InstanceBase.h       # Singleton<T> template base class
│   ├── camera.hpp           # First-person/LookAt camera
│   ├── threadpool.hpp       # Thread pool
│   ├── Entrypoints.h        # Platform entry macro VULKAN_EXAMPLE_MAIN
│   └── ...
├── src/nanite/              # Nanite core algorithms
│   ├── Const.h              # Global constants + Graph / MetisGraph / ClusterInfo / BVHNodeInfo
│   ├── Cluster.*            # Cluster data structure
│   ├── ClusterGroup.*       # ClusterGroup + METIS graph partitioning
│   ├── NaniteMesh.*         # Nanite mesh management (load/serialize/deserialize)
│   ├── NaniteLodMesh.*      # LOD mesh processing (simplification, BVH build, error calc)
│   ├── NaniteInstance.*     # Nanite instancing (world transform, BVH rebuild)
│   ├── NaniteScene.*        # Nanite scene (multi-mesh aggregation, GPU buffer creation)
│   ├── NaniteBVH.h          # BVH node definitions
│   ├── utils.*              # Assert + triangle AABB calculation
│   └── vksTools.*           # Resource creation helpers (StagingBuffer, IBL gen, descriptor setup)
├── examples/
│   └── pbrtexture/          # Main example (PBR + Nanite full pipeline)
├── shaders/glsl/pbrtexture/ # GLSL shader sources
├── external/                # Third-party deps (glm, imgui, tinygltf, ktx, stb, vulkan headers)
├── assets/                  # Models (glTF) and texture resources
├── CMakeLists.txt           # Top-level CMake
├── build_windows.bat        # Windows build script
└── documents/               # Design docs (Render Graph, sync, bug list)
```

### 1.3 Application Entry & Main Loop
**Entry macro (Windows):**
```cpp
// examples/pbrtexture/main.cpp
#include "Entrypoints.h"
#include "pbrTexture.h"
VULKAN_EXAMPLE_MAIN(PBRTexture)
```
`VULKAN_EXAMPLE_MAIN` expands to:
```
new PBRTexture() → initVulkan() → setupWindow() → prepare() → renderLoop() → delete
```

**Main render loop (`renderLoop()`):**
```
renderLoop() → nextFrame() → [calc deltaTime, update Camera] → render() → [present frame]
```

### 1.4 Rendering Pipeline Flow (Per Frame)
```
Scene Data → BVH Traversal → Error Projection → LOD Selection
  → Frustum Culling → HW Rasterization / SW Rasterization
  → Depth Copy → Hi-Z Generation → Occlusion Culling → Merge Rasterization Results
  → Deferred Shading → Skybox → Post-Processing → Final Image
```

## 2. Code Style & Consistency

### 2.1 Language Standard
- **C++20** (`CMAKE_CXX_STANDARD 20`). Use `std::format`, `std::source_location`, `[[nodiscard]]`, `constexpr if`, structured bindings etc.
- GLSL shaders use `#version 450` or higher.

### 2.2 Naming Conventions

| Category | Convention | Example |
|----------|-----------|---------|
| **Class/Struct** | PascalCase | `VulkanDevice`, `PBRTexture`, `NaniteLodMesh`, `ClusterGroup` |
| **Member functions** | camelCase | `createBuffer()`, `loadFromFile()`, `buildClusterGraph()` |
| **Member variables** | camelCase, no prefix | `pipelineLayout`, `vulkanDevice`, `naniteMesh` |
| **Private members** (Logger) | m_ prefix | `m_minLevel`, `m_consoleEnabled` |
| **Local variables** | camelCase | `cmdBuf`, `imageCI`, `subresourceRange` |
| **Constants** | UPPER_SNAKE_CASE or constexpr camelCase | `CLUSTER_TARGET_SIZE`, `DEFAULT_FENCE_TIMEOUT` |
| **Enum class** | PascalCase (name and members) | `DescriptorType::Scene`, `DescriptorType::bvhTraversal` |
| **Namespace** | PascalCase or lowercase | `Nanite`, `vks`, `vks::tools`, `vks::initializers`, `vkglTF` |
| **File names** | PascalCase .h/.cpp (base) or camelCase (examples) | `VulkanDevice.h`, `pbrTexture.h`, `NaniteLodMesh.cpp` |
| **Shader files** | lowercase camelCase or lowercase | `bvhtraversal.comp`, `genHiz.comp`, `hwrasterize.vert` |
| **Vulkan handles** | Initialize to `VK_NULL_HANDLE` | `VkPipeline pipeline{VK_NULL_HANDLE};` |

### 2.3 Header File Standards
- Use `#pragma once` as include guard (NOT `#ifndef` style).
- License header format (for base framework files):
```cpp
/*
 * Description
 *
 * Copyright (C) 2016-2025 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */
```
- Custom modules (`src/nanite/` and `examples/`) do not require license headers.

### 2.4 Comment Style
- Use `//` single-line comments primarily (Chinese or English both acceptable).
- Vulkan struct comments may use `/** @brief ... */` format (only in base/ module).
- No Doxygen format required, but key public APIs should have brief descriptions.

### 2.5 Type Hints & Modifiers
- Use `[[nodiscard]]` for functions whose return values should not be ignored.
- Use `const` and `noexcept` where appropriate.
- GPU-bound structs must use `alignas(N)` for alignment:
```cpp
alignas(16) glm::vec3 pMinWorld;  // vec3 aligned to 16 bytes
alignas(4)  uint32_t objectId;
alignas(8)  glm::vec2 errorWorld;
```
- Disable copy with `= delete`:
```cpp
PBRTexture(const PBRTexture&) = delete;
PBRTexture& operator=(const PBRTexture&) = delete;
```

### 2.6 Indentation & Formatting
- **Indentation: use Tab** (base framework style).
- Brace style: mixed Allman and K&R, follow existing code.
- Function implementations in `.cpp` use fully qualified class names.

## 3. Quick Reference & Context Mapping

### 3.1 Core Utility Function Signatures (Top 10 Most Used)

```cpp
// ---- VulkanDevice (base/VulkanDevice.h) ----
VkResult VulkanDevice::createBuffer(VkBufferUsageFlags, VkMemoryPropertyFlags, VkDeviceSize, VkBuffer*, VkDeviceMemory*, void* data = nullptr);
VkResult VulkanDevice::createBuffer(VkBufferUsageFlags, VkMemoryPropertyFlags, vks::Buffer*, VkDeviceSize, void* data = nullptr);
VkCommandBuffer VulkanDevice::createCommandBuffer(VkCommandBufferLevel, bool begin = false);
VkCommandBuffer VulkanDevice::createCommandBuffer(VkCommandBufferLevel, VkCommandPool, bool begin = false);
void VulkanDevice::flushCommandBuffer(VkCommandBuffer, VkQueue, bool free = true);
void VulkanDevice::flushCommandBuffer(VkCommandBuffer, VkQueue, VkCommandPool, bool free = true);
uint32_t VulkanDevice::getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags, VkBool32* memTypeFound = nullptr) const;

// ---- VulkanTools (base/VulkanTools.h) ----
void vks::tools::setImageLayout(VkCommandBuffer, VkImage, VkImageLayout old, VkImageLayout new, VkImageSubresourceRange, VkPipelineStageFlags src = ..., VkPipelineStageFlags dst = ...);
VkShaderModule vks::tools::loadShader(const char* fileName, VkDevice);
const std::string getAssetPath();
const std::string getShaderBasePath();

// ---- VulkanExampleBase (base/vulkanexamplebase.h) ----
VkPipelineShaderStageCreateInfo VulkanExampleBase::loadShader(std::string fileName, VkShaderStageFlagBits stage);
```

### 3.2 VulkanInitializers — Struct Init Factory (Most Used)
All in `vks::initializers` namespace (`base/VulkanInitializers.hpp`), all `inline`:
```cpp
vks::initializers::imageCreateInfo()
vks::initializers::imageViewCreateInfo()
vks::initializers::samplerCreateInfo()
vks::initializers::bufferCreateInfo()
vks::initializers::memoryAllocateInfo()
vks::initializers::descriptorSetLayoutBinding(VkDescriptorType, VkShaderStageFlags, uint32_t binding, uint32_t count = 1)
vks::initializers::writeDescriptorSet(VkDescriptorSet, VkDescriptorType, uint32_t binding, VkDescriptorBufferInfo*, uint32_t count = 1)
vks::initializers::pipelineCreateInfo(VkPipelineLayout, VkRenderPass, VkPipelineCreateFlags = 0)
vks::initializers::computePipelineCreateInfo(VkPipelineLayout, VkPipelineCreateFlags = 0)
vks::initializers::pushConstantRange(VkShaderStageFlags, uint32_t size, uint32_t offset)
vks::initializers::viewport(float w, float h, float minDepth, float maxDepth)
vks::initializers::rect2D(int32_t w, int32_t h, int32_t offsetX, int32_t offsetY)
```

### 3.3 Key Constants
```cpp
// Nanite core constants (src/nanite/Const.h)
constexpr int Nanite::CLUSTER_TARGET_SIZE = 56;
constexpr int Nanite::CLUSTER_MAX_SIZE = 64;
constexpr int Nanite::CLUSTER_GROUP_TARGET_SIZE = 15;
constexpr int Nanite::CLUSTER_GROUP_MAX_SIZE = 32;

// Compute shader workgroup sizes (examples/pbrtexture/pbrTexture.h)
static constexpr int WORKGROUP_SIZE_X = 8;
static constexpr int WORKGROUP_SIZE_Y = 8;
static constexpr int DISPATCH_GROUP_SIZE = 32;

// Vulkan global defines (base/VulkanTools.h)
#define VK_FLAGS_NONE 0
#define DEFAULT_FENCE_TIMEOUT 100000000000  // nanoseconds (100s)
```

### 3.4 DescriptorType Enum
```cpp
// base/VulkanDescriptorManager.h
enum class DescriptorType {
    Scene, hiz, depthCopy, debugQuad, culling, errorPorj,
    hwRast, swRast, clearImage, mergeRast, bvhTraversal, shading,
};
```
> **Note:** `errorPorj` is a known spelling (not `errorProj`). Do NOT "fix" this name unless explicitly refactoring.

### 3.5 Global Object Access

| Target | Access | Notes |
|--------|--------|-------|
| Vulkan device | `vulkanDevice` (member) | `vks::VulkanDevice*` in `VulkanExampleBase` |
| Logical device | `vulkanDevice->logicalDevice` or `device` | `VkDevice` |
| Graphics queue | `queue` (member) or `GetQueue()` | `VkQueue` |
| Descriptor manager | `VulkanDescriptorManager::getManager()` | Singleton |
| Camera | `camera` (member) | `Camera`, supports `firstperson` / `lookat` |
| Asset paths | `getAssetPath()` / `getShaderBasePath()` / `getShadersPath()` | Global and member functions |
| Pipeline cache | `GetPipelineCache()` or `pipelineCache` | `VkPipelineCache` |
| Nanite scene | `scene` (PBRTexture member) | `Nanite::NaniteScene` |
| Nanite mesh | `naniteMesh` (PBRTexture member) | `Nanite::NaniteMesh` |

## 4. Rendering/Game-Specific Patterns

### 4.1 Async & Concurrency
- **GPU sync uses Pipeline Barriers:** All cross-pass resource dependencies use `VkBufferMemoryBarrier` and `VkImageMemoryBarrier`.
- Helper creation functions:
```cpp
[[nodiscard]] static VkBufferMemoryBarrier createBufferBarrier(VkBuffer, VkAccessFlags src, VkAccessFlags dst);
[[nodiscard]] static VkImageMemoryBarrier createImageBarrier(VkImage, VkImageLayout old, VkImageLayout new, VkAccessFlags src, VkAccessFlags dst, const VkImageSubresourceRange&);
```
- **Data upload uses Staging Buffer pattern:**
```cpp
vks::vksTools::createStagingBuffer(variableLink, srcUsage, size, data, dstUsage, targetBuffer);
```

### 4.2 Lifecycle Management

**Strict lifecycle order:**
```
Constructor → initVulkan() → prepare()
  → loadAssets() → createNaniteScene() → prepareUniformBuffers()
  → setupDescriptors() → preparePipelines()
→ renderLoop() → render() (per frame: update UBO + buildCommandBuffers)
→ ~Destructor() (destroy all resources)
```

**Destruction rules (critical):**
1. In destructor, **must check `if (!device) return;` first**.
2. Destroy in **reverse creation order**.
3. **Pipeline → PipelineLayout → DescriptorManager → Textures → Buffers → ImageViews**.
4. Use `Pipeline::destroy(device)` wrapper.
5. When destroying ImageViews in a loop, **clear the container**:
```cpp
for (auto& imageView : hizImageViews) {
    vkDestroyImageView(device, imageView, nullptr);
}
hizImageViews.clear();
```

### 4.3 Vulkan Resource Creation Patterns

**Buffer creation standard:**
```cpp
VK_CHECK_RESULT(vulkanDevice->createBuffer(
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    &myBuffer, bufferSize, initialData));
myBuffer.setupDescriptor();
```

**Descriptor setup standard:**
```cpp
auto descMgr = VulkanDescriptorManager::getManager();
descMgr->addSetLayout(DescriptorType::XXX, setLayoutBindings, numSets);
descMgr->createLayoutsAndSets(device);
descMgr->writeToSet(DescriptorType::XXX, setIndex, binding, &bufferInfo);
```

### 4.4 Performance Rules
1. **GPU Buffers use `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`**: Upload via Staging Buffer, no host visible memory for render resources.
2. **Compute Shader Dispatch**: workgroup = 8x8, dispatch group = 32, ceil division: `(count + 31) / 32`.
3. **Hi-Z Buffer** for accelerated occlusion culling, per-mip downsampling.
4. **Indirect Draw/Dispatch**: GPU-driven drawing, avoid CPU-GPU sync.
5. **Nanite cache**: `initNaniteInfo(filepath, useCache=true)` prefers disk deserialization.
6. **`alignas`**: All GPU-bound structs must satisfy std140/std430 alignment.

### 4.5 Error Handling
**All Vulkan API calls must use `VK_CHECK_RESULT` macro:**
```cpp
VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &image));
```

**Nanite assertions:**
```cpp
Nanite::NaniteAssert(condition, "error message");
// Uses std::source_location, calls std::abort() on failure
```

## 5. Developer Workflow

### 5.1 Build
```bash
# Windows
mkdir build && cd build
cmake -D VCPKG_TARGET_TRIPLET=x64-windows-static \
      -D CMAKE_TOOLCHAIN_FILE=../vcpkglib/vcpkg.windows/scripts/buildsystems/vcpkg.cmake \
      -G "Visual Studio 17 2022" -A "x64" ..
msbuild cyVulkanNanite.sln /p:Configuration=Release
# Or: build_windows.bat
```

### 5.2 GLSL Shader Compilation
```bash
%VK_SDK_PATH%/Bin/glslangValidator.exe -V input.comp -o input.comp.spv -g
# Or batch: python shaders/glsl/compileshaders.py
# CMake auto-compiles on rebuild, tracks .glsl include deps
```

### 5.3 Adding a New Example
1. Create `examples/myexample/` with `main.cpp`, `myexample.h`, `myexample.cpp`.
2. Add `myexample` to `EXAMPLES` list in `examples/CMakeLists.txt`.
3. Add shaders in `shaders/glsl/myexample/`.

## 6. Critical Safety Rules

1. **Style consistency is law:** Match existing indentation (Tab), import ordering, naming style exactly.
2. **Zero hallucination:** Only reference APIs, constants, and classes that actually exist in the codebase.
3. **GPU struct alignment:** When modifying C++ structs passed to shaders, sync GLSL definitions and verify `alignas`.
4. **Vulkan resources must be paired:** Every `vkCreate*` needs a `vkDestroy*`, every `vkAllocate*` needs a `vkFree*`.
5. **Descriptor binding consistency:** When modifying bindings, check both `vksTools::setPbrDescriptor()` binding numbers and shader `layout(binding=N)`.
6. **Do NOT "fix"** known naming inconsistencies (e.g., `errorPorj`) unless user explicitly requests refactoring.
7. **Pipeline Barriers must not be omitted:** Insert appropriate buffer/image memory barriers between Compute and Graphics passes.
