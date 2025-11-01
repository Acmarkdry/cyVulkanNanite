# Copyright (c) 2016-2025, Sascha Willems
# SPDX-License-Identifier: MIT

file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h" "../external/imgui/*.cpp")
file(GLOB BASE_HEADERS "*.hpp" "*.h")

set(KTX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/ktx)
set(KTX_SOURCES
    ${KTX_DIR}/lib/texture.c
    ${KTX_DIR}/lib/hashlist.c
    ${KTX_DIR}/lib/checkheader.c
    ${KTX_DIR}/lib/swap.c
    ${KTX_DIR}/lib/memstream.c
    ${KTX_DIR}/lib/filestream.c
    ${KTX_DIR}/lib/vkloader.c)

add_library(base STATIC ${BASE_SRC} ${KTX_SOURCES})

# 使用 target 级别的 include 路径（PUBLIC 暴露给链接 base 的目标）
target_include_directories(base PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/external/glm
    ${CMAKE_SOURCE_DIR}/external/gli
    ${CMAKE_SOURCE_DIR}/external/imgui
    ${CMAKE_SOURCE_DIR}/external/tinygltf
    ${CMAKE_SOURCE_DIR}/external/ktx/include
    ${CMAKE_SOURCE_DIR}/external/ktx/other_include
)

if(WIN32)
    target_link_libraries(base PUBLIC 
        ${Vulkan_LIBRARY} 
        ${WINLIBS}
        nanite_core
    )
else(WIN32)
    target_link_libraries(base PUBLIC
        ${Vulkan_LIBRARY} 
        ${XCB_LIBRARIES} 
        ${WAYLAND_CLIENT_LIBRARIES} 
        ${CMAKE_THREAD_LIBS_INIT}
        nanite_core
    )
endif(WIN32)