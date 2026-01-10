# src/RenderGraph/CMakeLists.txt
# RenderGraph 测试可执行文件

file(GLOB RENDER_GRAPH_SOURCES "private/*.cpp")
file(GLOB RENDER_GRAPH_HEADERS "public/*.h")

# 改为可执行文件
add_executable(render_graph_test ${RENDER_GRAPH_SOURCES} ${RENDER_GRAPH_HEADERS})

# include 路径
# 注意：
# 1. external/glm (0.9.9.8) 必须在 external/gli 之前，确保项目代码拿到正确版本
# 2. external/gli 整体作为 SYSTEM include，降低其内嵌旧版 GLM 的路径优先级
#    并配合 GLM_FORCE_IGNORE_VERSION 避免两个版本同时 include 时报版本冲突错误
target_include_directories(render_graph_test PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/public
    ${CMAKE_SOURCE_DIR}/external/glm
)
target_include_directories(render_graph_test SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/external/gli
)

# 打印找到的源文件
message(STATUS "Found sources: ${RENDER_GRAPH_SOURCES}")

# GLM 配置宏：
# - GLM_FORCE_EXPLICIT_CTOR: 0.9.9.8 严格模式，要求显式转换
# - GLM_ENABLE_EXPERIMENTAL: 启用 GLM GTX 扩展
# - GLM_FORCE_IGNORE_VERSION: 允许 GLI 内嵌的旧版 GLM 与 0.9.9.8 共存，
#   避免 setup.hpp 触发 "A different version of GLM is already included" 错误
target_compile_definitions(render_graph_test PRIVATE
    GLM_FORCE_EXPLICIT_CTOR
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_IGNORE_VERSION
)

# 链接 base 库
target_link_libraries(render_graph_test PRIVATE base)

# 设置输出目录（可选）
set_target_properties(render_graph_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

message(STATUS "render_graph_test: executable from ${CMAKE_CURRENT_SOURCE_DIR}")