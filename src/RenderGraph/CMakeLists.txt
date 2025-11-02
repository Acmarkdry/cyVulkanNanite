# src/RenderGraph/CMakeLists.txt
# RenderGraph 静态库 — 渲染图抽象层

file(GLOB RENDER_GRAPH_SOURCES "*.cpp")
file(GLOB RENDER_GRAPH_HEADERS "*.h")

add_library(render_graph STATIC ${RENDER_GRAPH_SOURCES} ${RENDER_GRAPH_HEADERS})

# 暴露 include 路径给依赖者
# PUBLIC: 任何链接 render_graph 的目标都能直接 #include "RenderGraph.h" 等
target_include_directories(render_graph PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 链接 base 库（传递依赖：base -> nanite_core -> OpenMesh/METIS）
# base 以 PUBLIC 方式暴露了以下 include 路径和链接：
#   - base/ 头文件（logger.h, VulkanDevice.h, VulkanBuffer.h 等）
#   - Vulkan SDK
#   - external/（glm, imgui, tinygltf, ktx）
#   - nanite_core 头文件（utils.h / NaniteAssert 等）
target_link_libraries(render_graph PUBLIC base)

message(STATUS "render_graph: sources in ${CMAKE_CURRENT_SOURCE_DIR}")
