# src/nanite/CMakeLists.txt
# Nanite Core 静态库 — Nanite LOD 网格处理核心

file(GLOB NANITE_SOURCES "*.cpp")
file(GLOB NANITE_HEADERS "*.h")

add_library(nanite_core STATIC ${NANITE_SOURCES} ${NANITE_HEADERS})

# 暴露 include 路径给依赖者
# PUBLIC: 任何链接 nanite_core 的目标都能直接 #include "NaniteMesh.h" 等
target_include_directories(nanite_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# nanite_core 需要 base 的头文件 (VulkanglTFModel.h 等)
# 但不链接 base 库本身（避免循环依赖），仅传递 include 路径
target_include_directories(nanite_core PUBLIC
    ${CMAKE_SOURCE_DIR}/base
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/external/glm
    ${CMAKE_SOURCE_DIR}/external/gli
    ${CMAKE_SOURCE_DIR}/external/tinygltf
    ${CMAKE_SOURCE_DIR}/external/ktx/include
    ${CMAKE_SOURCE_DIR}/external/ktx/other_include
    ${Vulkan_INCLUDE_DIRS}
)

# 第三方依赖
find_package(OpenMesh CONFIG REQUIRED)
find_package(metis CONFIG REQUIRED)

target_link_libraries(nanite_core PUBLIC
    OpenMeshCore
    OpenMeshTools
    metis
)

message(STATUS "nanite_core: sources in ${CMAKE_CURRENT_SOURCE_DIR}")
