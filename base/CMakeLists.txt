# Copyright (c) 2016-2025, Sascha Willems
# SPDX-License-Identifier: MIT

file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h" "../external/imgui/*.cpp")
file(GLOB BASE_HEADERS "*.hpp" "*.h")
file(GLOB NaniteMesh "../src/NaniteMesh/*.cpp" "../src/NaniteMesh/*.h")
file(GLOB Common "../src/*.cpp" "../src/*.h")

find_package(OpenMesh CONFIG REQUIRED)

# 打印一下找到的内容，方便排查 (在 CMake 输出窗口查看)
message(STATUS "OpenMesh Found: ${OpenMesh_FOUND}")
message(STATUS "OpenMesh Libs: ${OPENMESH_INCLUDE_DIR}")
message(STATUS "OpenMesh Include: ${OPENMESH_LIBRARIES}")

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

# ========================================================
# [修改 2] 显式包含头文件路径
# 防止编译时报 "OpenMesh/Core/Mesh/..." not found
# ========================================================
target_include_directories(base PUBLIC ${OPENMESH_INCLUDE_DIR})

if(WIN32)
    target_link_libraries(base PUBLIC 
        ${Vulkan_LIBRARY} 
        ${WINLIBS} 
        ${OPENMESH_LIBRARIES}
    )
else(WIN32)
    target_link_libraries(base 
        ${Vulkan_LIBRARY} 
        ${XCB_LIBRARIES} 
        ${WAYLAND_CLIENT_LIBRARIES} 
        ${CMAKE_THREAD_LIBS_INIT}
        ${OPENMESH_LIBRARIES}
    )
endif(WIN32)