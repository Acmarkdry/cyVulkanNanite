# src/nanite/CMakeLists.txt
# Nanite Core 静态库 — Nanite LOD 网格处理核心

file(GLOB NANITE_SOURCES CONFIGURE_DEPENDS "*.cpp")
file(GLOB NANITE_HEADERS CONFIGURE_DEPENDS "*.h")

add_library(nanite_core STATIC ${NANITE_SOURCES} ${NANITE_HEADERS})

# ── PUBLIC include 路径 ──────────────────────────────────────
# nanite 自身头文件路径
target_include_directories(nanite_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# nanite 公共头文件的传递依赖：
#   NaniteLodMesh.h → VulkanglTFModel.h → vulkan.h, ktx.h, tiny_gltf.h
#   NaniteMesh.h    → tiny_gltf.h
#   Const.h         → glm.hpp, OpenMesh, metis.h
#   NaniteBVH.h     → json.hpp
# 这些路径必须 PUBLIC，否则链接者 include nanite 头文件时编译失败
target_include_directories(nanite_core PUBLIC
    ${CMAKE_SOURCE_DIR}/base              # VulkanglTFModel.h, VulkanDevice.h
    ${CMAKE_SOURCE_DIR}/external          # json.hpp
    ${CMAKE_SOURCE_DIR}/external/glm      # glm
    ${CMAKE_SOURCE_DIR}/external/tinygltf # tiny_gltf.h
    ${CMAKE_SOURCE_DIR}/external/ktx/include       # ktx.h
    ${CMAKE_SOURCE_DIR}/external/ktx/other_include # ktxvulkan.h
    ${Vulkan_INCLUDE_DIRS}                # vulkan/vulkan.h
)

# ── PRIVATE include 路径 ─────────────────────────────────────
# 仅 nanite_core .cpp 文件内部使用，不被任何公共头文件引用
target_include_directories(nanite_core PRIVATE
    ${CMAKE_SOURCE_DIR}/external/gli      # gli（仅 .cpp 中使用，公共头文件不引用）
)

# ── 第三方依赖 ───────────────────────────────────────────────
find_package(OpenMesh CONFIG REQUIRED)
find_package(metis CONFIG REQUIRED)

target_link_libraries(nanite_core PUBLIC
    OpenMeshCore
    OpenMeshTools
    metis
)

message(STATUS "nanite_core: sources in ${CMAKE_CURRENT_SOURCE_DIR}")
