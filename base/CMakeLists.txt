# Copyright (c) 2016-2025, Sascha Willems
# SPDX-License-Identifier: MIT

file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h" "../external/imgui/*.cpp")
file(GLOB BASE_HEADERS "*.hpp" "*.h")
file(GLOB NaniteMesh "../src/NaniteMesh/*.cpp" "../src/NaniteMesh/*.h")
file(GLOB Common "../src/*.cpp" "../src/*.h")

find_package(OpenMesh CONFIG REQUIRED)

# 正确的调试信息（imported target 不提供这些变量）
message(STATUS "OpenMesh Found: ${OpenMesh_FOUND}")
find_package(metis CONFIG REQUIRED)
message(STATUS "Metis Found: ${metis_FOUND}") 


set(KTX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/ktx)
set(KTX_SOURCES
    ${KTX_DIR}/lib/texture.c
    ${KTX_DIR}/lib/hashlist.c
    ${KTX_DIR}/lib/checkheader.c
    ${KTX_DIR}/lib/swap.c
    ${KTX_DIR}/lib/memstream.c
    ${KTX_DIR}/lib/filestream.c
    ${KTX_DIR}/lib/vkloader.c)

add_library(base STATIC ${BASE_SRC} ${KTX_SOURCES} ${Common} ${NaniteMesh})


if(WIN32)
    target_link_libraries(base PUBLIC 
        ${Vulkan_LIBRARY} 
        ${WINLIBS}
        OpenMeshCore      # <-- 使用 imported target，会自动传递 include 路径
        OpenMeshTools     # <-- 同上
        metis
    )
else(WIN32)
    target_link_libraries(base PUBLIC
        ${Vulkan_LIBRARY} 
        ${XCB_LIBRARIES} 
        ${WAYLAND_CLIENT_LIBRARIES} 
        ${CMAKE_THREAD_LIBS_INIT}
        OpenMeshCore
        OpenMeshTools
    )
endif(WIN32)
