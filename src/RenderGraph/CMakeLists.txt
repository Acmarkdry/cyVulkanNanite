# src/RenderGraph/CMakeLists.txt
# RenderGraph 测试可执行文件

file(GLOB RENDER_GRAPH_SOURCES "private/*.cpp")
file(GLOB RENDER_GRAPH_HEADERS "public/*.h")

# 改为可执行文件
add_executable(render_graph_test ${RENDER_GRAPH_SOURCES} ${RENDER_GRAPH_HEADERS})

# include 路径
target_include_directories(render_graph_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 打印找到的源文件
message(STATUS "Found sources: ${RENDER_GRAPH_SOURCES}")

# 链接 base 库
target_link_libraries(render_graph_test PRIVATE base)

# 设置输出目录（可选）
set_target_properties(render_graph_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

message(STATUS "render_graph_test: executable from ${CMAKE_CURRENT_SOURCE_DIR}")