#=============================================================================
# 公共 GLSL Shader 编译函数
#=============================================================================
# 编译单个 GLSL shader 为 SPIR-V，并自动追踪同目录下的 .glsl include 文件
# 用法: compile_glsl_shader(SHADER_SOURCE SPIRV_LIST_VAR)
function(compile_glsl_shader SHADER_SOURCE SPIRV_LIST_VAR)
    get_filename_component(fname ${SHADER_SOURCE} NAME)
    get_filename_component(shader_dir ${SHADER_SOURCE} DIRECTORY)

    set(SPIRV_OUTPUT "${shader_dir}/${fname}.spv")

    # 收集同目录下的 .glsl include 头文件作为额外依赖
    file(GLOB GLSL_INCLUDES "${shader_dir}/*.glsl")

    add_custom_command(
        OUTPUT ${SPIRV_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${shader_dir}
        COMMAND $ENV{VK_SDK_PATH}/Bin/glslangValidator.exe -V ${SHADER_SOURCE} -o ${SPIRV_OUTPUT} -g
        DEPENDS ${SHADER_SOURCE} ${GLSL_INCLUDES}
        COMMENT "Compiling ${fname} -> SPIR-V"
    )

    # 追加到调用者的列表变量
    set(${SPIRV_LIST_VAR} "${${SPIRV_LIST_VAR}};${SPIRV_OUTPUT}" PARENT_SCOPE)
endfunction()

# 批量编译指定目录下的所有 GLSL shader
# 用法: compile_glsl_shaders(SHADER_DIR SPIRV_LIST_VAR)
function(compile_glsl_shaders SHADER_DIR SPIRV_LIST_VAR)
    file(GLOB SHADER_SOURCES
        "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag" "${SHADER_DIR}/*.comp"
        "${SHADER_DIR}/*.geom" "${SHADER_DIR}/*.tesc" "${SHADER_DIR}/*.tese"
        "${SHADER_DIR}/*.mesh" "${SHADER_DIR}/*.task"
        "${SHADER_DIR}/*.rgen" "${SHADER_DIR}/*.rchit" "${SHADER_DIR}/*.rmiss"
        "${SHADER_DIR}/*.rcall" "${SHADER_DIR}/*.rahit" "${SHADER_DIR}/*.rint")

    set(_spirv_deps "")
    foreach(SHADER_SOURCE ${SHADER_SOURCES})
        compile_glsl_shader(${SHADER_SOURCE} _spirv_deps)
    endforeach()

    set(${SPIRV_LIST_VAR} "${_spirv_deps}" PARENT_SCOPE)
endfunction()

#=============================================================================
# Example 构建函数
#=============================================================================
function(buildExample EXAMPLE_NAME)
	SET(EXAMPLE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_NAME})
	message(STATUS "Generating project file for example in ${EXAMPLE_FOLDER}")

	# 源文件
	file(GLOB SOURCE ${EXAMPLE_FOLDER}/*.cpp ${EXAMPLE_FOLDER}/*.h)
	SET(MAIN_CPP ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.cpp)
	if(EXISTS ${EXAMPLE_FOLDER}/main.cpp)
		SET(MAIN_CPP ${EXAMPLE_FOLDER}/main.cpp)
	ENDIF()
	if(EXISTS ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.h)
		SET(MAIN_HEADER ${EXAMPLE_FOLDER}/${EXAMPLE_NAME}.h)
	ENDIF()

	# imgui example requires additional source files
	IF(${EXAMPLE_NAME} STREQUAL "imgui")
		file(GLOB ADD_SOURCE "../external/imgui/*.cpp")
		SET(SOURCE ${SOURCE} ${ADD_SOURCE})
	ENDIF()

	# wayland requires additional source files
	IF(USE_WAYLAND_WSI)
		SET(SOURCE ${SOURCE} ${CMAKE_BINARY_DIR}/xdg-shell-client-protocol.h ${CMAKE_BINARY_DIR}/xdg-shell-protocol.c)
	ENDIF()

	# Shader 文件（用于 IDE 显示 + 编译）
	set(SHADER_DIR_GLSL "${CMAKE_SOURCE_DIR}/shaders/glsl/${EXAMPLE_NAME}")
	file(GLOB SHADERS_GLSL
		"${SHADER_DIR_GLSL}/*.vert" "${SHADER_DIR_GLSL}/*.frag" "${SHADER_DIR_GLSL}/*.comp"
		"${SHADER_DIR_GLSL}/*.geom" "${SHADER_DIR_GLSL}/*.tesc" "${SHADER_DIR_GLSL}/*.tese"
		"${SHADER_DIR_GLSL}/*.mesh" "${SHADER_DIR_GLSL}/*.task"
		"${SHADER_DIR_GLSL}/*.rgen" "${SHADER_DIR_GLSL}/*.rchit" "${SHADER_DIR_GLSL}/*.rmiss"
		"${SHADER_DIR_GLSL}/*.rcall" "${SHADER_DIR_GLSL}/*.rahit" "${SHADER_DIR_GLSL}/*.rint")
	file(GLOB SHADER_INCLUDES_GLSL "${SHADER_DIR_GLSL}/*.glsl")
	source_group("Shaders\\GLSL" FILES ${SHADERS_GLSL} ${SHADER_INCLUDES_GLSL})

	# HLSL (如果存在)
	set(SHADER_DIR_HLSL "${CMAKE_SOURCE_DIR}/shaders/hlsl/${EXAMPLE_NAME}")
	file(GLOB SHADERS_HLSL "${SHADER_DIR_HLSL}/*.vert" "${SHADER_DIR_HLSL}/*.frag" "${SHADER_DIR_HLSL}/*.comp" "${SHADER_DIR_HLSL}/*.geom")
	source_group("Shaders\\HLSL" FILES ${SHADERS_HLSL})

	# Readme
	file(GLOB README_FILES "${EXAMPLE_FOLDER}/*.md")

	if(WIN32)
		add_executable(${EXAMPLE_NAME} WIN32 ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS_GLSL} ${SHADER_INCLUDES_GLSL} ${SHADERS_HLSL} ${README_FILES})
		target_link_libraries(${EXAMPLE_NAME} base ${Vulkan_LIBRARY} ${WINLIBS})

		# 编译 GLSL → SPIR-V（使用公共函数，自动追踪 .glsl 依赖）
		set(SPIRV_DEPS "")
		compile_glsl_shaders(${SHADER_DIR_GLSL} SPIRV_DEPS)
		add_custom_target(${EXAMPLE_NAME}_shader_spv DEPENDS ${SPIRV_DEPS})
		add_dependencies(${EXAMPLE_NAME} ${EXAMPLE_NAME}_shader_spv base_shader_spv)
		set_target_properties(${EXAMPLE_NAME}_shader_spv PROPERTIES FOLDER "CMakePredefinedTargets")
	else(WIN32)
		add_executable(${EXAMPLE_NAME} ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS_GLSL} ${SHADER_INCLUDES_GLSL} ${SHADERS_HLSL} ${README_FILES})
		target_link_libraries(${EXAMPLE_NAME} base)
	endif(WIN32)

	file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
	set_target_properties(${EXAMPLE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

	if(${EXAMPLE_NAME} STREQUAL "texture3d")
		if(APPLE)
			if(NOT OpenMP_omp_LIBRARY AND EXISTS /opt/local/lib/libomp/libomp.dylib)
				set(OpenMP_omp_LIBRARY /opt/local/lib/libomp/libomp.dylib)
			endif()
			if(CMAKE_C_COMPILER_ID MATCHES "Clang\$")
				set(OpenMP_C_FLAGS "-Xclang -fopenmp")
				set(OpenMP_C_LIB_NAMES "omp")
				if(NOT OpenMP_C_INCLUDE_DIR AND EXISTS /opt/local/include/libomp)
					set(OpenMP_C_INCLUDE_DIR /opt/local/include/libomp)
				endif()
			endif()
			if(CMAKE_CXX_COMPILER_ID MATCHES "Clang\$")
				set(OpenMP_CXX_FLAGS "-Xclang -fopenmp")
				set(OpenMP_CXX_LIB_NAMES "omp")
				if(NOT OpenMP_CXX_INCLUDE_DIR AND EXISTS /opt/local/include/libomp)
					set(OpenMP_CXX_INCLUDE_DIR /opt/local/include/libomp)
				endif()
			endif()
		endif()
		find_package(OpenMP)
		if(OpenMP_CXX_FOUND)
			link_directories(${OpenMP_CXX_LIBRARY_DIRS})
		endif()
	endif()

	if(RESOURCE_INSTALL_DIR)
		install(TARGETS ${EXAMPLE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()
endfunction(buildExample)

#=============================================================================
# Base Shader 编译
#=============================================================================
function(compileBaseShaders)
	set(SHADER_DIR_GLSL "${CMAKE_SOURCE_DIR}/shaders/glsl/base")
	set(SPIRV_DEPS "")
	compile_glsl_shaders(${SHADER_DIR_GLSL} SPIRV_DEPS)
	add_custom_target(base_shader_spv DEPENDS ${SPIRV_DEPS})
endfunction(compileBaseShaders)

#=============================================================================
# 构建所有 Examples
#=============================================================================
set(EXAMPLES
	pbrtexture
)

compileBaseShaders()

foreach(EXAMPLE ${EXAMPLES})
	buildExample(${EXAMPLE})
endforeach(EXAMPLE)