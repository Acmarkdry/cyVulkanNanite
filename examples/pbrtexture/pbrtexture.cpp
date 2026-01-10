/*
* Vulkan Example - Physical based rendering a textured object (metal/roughness workflow) with image based lighting
*
* Copyright (C) 2016-2025 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

// For reference see http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
#include "logger.h"
#include "pbrTexture.h"
#include "VulkanDescriptorManager.h"
#include "../../src/vksTools.h"
#include "../../src/NaniteMesh/NaniteMesh.h"
#include "../../src/NaniteMesh/NaniteInstance.h"
#include "../../src/NaniteMesh/NaniteLodMesh.h"


PBRTexture::PBRTexture():VulkanExampleBase(true)
{
	title = "Textured PBR with IBL";
	camera.type = Camera::CameraType::firstperson;
	camera.movementSpeed = 4.0f;
	camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
	camera.rotationSpeed = 0.25f;
	camera.setRotation({ -7.75f, 150.25f, 0.0f });
	camera.setPosition({ 0.7f, 0.1f, 1.7f });
}

PBRTexture::~PBRTexture()
{
	if (device)
	{
		vkDestroyPipeline(device, pipelines.skybox, nullptr);
		vkDestroyPipeline(device, pipelines.pbr, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		
		auto descMgr = VulkanDescriptorManager::getManager();
		descMgr->destroy();
		textures.destroy();
		uniformBuffers.destroy();
		
		for (auto& hizImageView : hizImageViews)
		{
			vkDestroyImageView(device, hizImageView, nullptr);
		}
		hizImageViews.clear();
		
		hizComputePipeline.destroy(device);
		depthCopyPipeline.destroy(device);
		debugQuadPipeline.destroy(device);
	}
}

void PBRTexture::getEnabledFeatures()
{
	if (deviceFeatures.samplerAnisotropy)
	{
		enabledFeatures.samplerAnisotropy = VK_TRUE;
	}
}

void PBRTexture::loadAssets()
{
	constexpr uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
	models.skybox.loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, queue, glTFLoadingFlags);
	// models.object.loadFromFile(getAssetPath() + "models/cerberus/cerberus.gltf", vulkanDevice, queue, glTFLoadingFlags);
	models.object.loadFromFile(getAssetPath() + "models/bunny.gltf", vulkanDevice, queue, glTFLoadingFlags);
	
	naniteMesh.setModelPath((getAssetPath() + "models/bunny/").c_str());
	naniteMesh.loadvkglTFModel(models.object);
	naniteMesh.initNaniteInfo(getAssetPath() + "models/bunny.gltf", true);
	for (int i = 0; i < naniteMesh.meshes.size(); ++i)
	{
		naniteMesh.meshes[i].initUniqueVertexBuffer();
		naniteMesh.meshes[i].initVertexBuffer();
		naniteMesh.meshes[i].createVertexBuffer(*this);
	}
	
	createNaniteScene();
	
	textures.environmentCube.loadFromFile(getAssetPath() + "textures/hdr/gcanyon_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice, queue);
	textures.albedoMap.loadFromFile(getAssetPath() + "models/cerberus/albedo.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
	textures.normalMap.loadFromFile(getAssetPath() + "models/cerberus/normal.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
	textures.aoMap.loadFromFile(getAssetPath() + "models/cerberus/ao.ktx", VK_FORMAT_R8_UNORM, vulkanDevice, queue);
	textures.metallicMap.loadFromFile(getAssetPath() + "models/cerberus/metallic.ktx", VK_FORMAT_R8_UNORM, vulkanDevice, queue);
	textures.roughnessMap.loadFromFile(getAssetPath() + "models/cerberus/roughness.ktx", VK_FORMAT_R8_UNORM, vulkanDevice, queue);
}

void PBRTexture::setupDescriptors()
{
	vks::vksTools::setPbrDescriptor(*this);
}

void PBRTexture::preparePipelines()
{
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
	VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
	VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
	std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

	auto descManager = VulkanDescriptorManager::getManager();

	// Pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout(DescriptorType::Scene), 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

	// Pipelines
	VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
	pipelineCI.pInputAssemblyState = &inputAssemblyState;
	pipelineCI.pRasterizationState = &rasterizationState;
	pipelineCI.pColorBlendState = &colorBlendState;
	pipelineCI.pMultisampleState = &multisampleState;
	pipelineCI.pViewportState = &viewportState;
	pipelineCI.pDepthStencilState = &depthStencilState;
	pipelineCI.pDynamicState = &dynamicState;
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();
	pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({
		vkglTF::VertexComponent::Position,
		vkglTF::VertexComponent::Normal,
		vkglTF::VertexComponent::UV,
		vkglTF::VertexComponent::Tangent,
		vkglTF::VertexComponent::Joint0,
		vkglTF::VertexComponent::Weight0
	});

	// Skybox pipeline (background cube)
	rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
	shaderStages[0] = loadShader(getShadersPath() + "pbrtexture/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getShadersPath() + "pbrtexture/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.skybox));

	// PBR pipeline
	rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
	shaderStages[0] = loadShader(getShadersPath() + "pbrtexture/pbrtexture.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getShadersPath() + "pbrtexture/pbrtexture.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	// Enable depth test and write
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthTestEnable = VK_TRUE;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.pbr));

	// debug quad buffer 
	{
		pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout(DescriptorType::debugQuad), 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &debugQuadPipeline.pipelineLayout));

		pipelineCI.layout = debugQuadPipeline.pipelineLayout;
		pipelineCI.renderPass = renderPass;
		// 作为一个debug设置，关闭深度写入
		depthStencilState.depthWriteEnable = VK_FALSE;
		depthStencilState.depthTestEnable = VK_FALSE;
		VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::pipelineVertexInputStateCreateInfo();
		pipelineCI.pVertexInputState = &vertexInputStateCI;
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		shaderStages[0] = loadShader(getShadersPath() + "pbrtexture/debugQuad.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "pbrtexture/debugQuad.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineCI.stageCount = 2;
		pipelineCI.pStages = shaderStages.data();
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &debugQuadPipeline.pipeline));
	}

	// hiz buffer
	{
		VkPipelineShaderStageCreateInfo computeShaderStage = loadShader(getShadersPath() + "pbrtexture/genHiz.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout(DescriptorType::hiz), 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &hizComputePipeline.pipelineLayout));

		VkComputePipelineCreateInfo pipelineCreateInfo = vks::initializers::computePipelineCreateInfo(hizComputePipeline.pipelineLayout);
		pipelineCreateInfo.stage = computeShaderStage;
		VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &hizComputePipeline. pipeline));
	}

	// depth copy
	{
		VkPipelineShaderStageCreateInfo computeShaderStage = loadShader(getShadersPath() + "pbrtexture/depthCopy.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout(DescriptorType::depthCopy), 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &depthCopyPipeline.pipelineLayout));

		VkComputePipelineCreateInfo pipelineCreateInfo = vks::initializers::computePipelineCreateInfo(depthCopyPipeline.pipelineLayout);
		pipelineCreateInfo.stage = computeShaderStage;
		VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &depthCopyPipeline.pipeline ));
	}

	// culling
	{
		VkPipelineShaderStageCreateInfo computeShaderStage = loadShader(getShadersPath() + "pbrtexture/culling.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		VkPushConstantRange pushConstant = {};
		pushConstant.size = sizeof(cullingPushConstants);
		pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout(DescriptorType::culling), 1);
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &cullingPipeline.pipelineLayout))

		VkComputePipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.layout = cullingPipeline.pipelineLayout;
		pipelineCreateInfo.stage = computeShaderStage;
		VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &cullingPipeline.pipeline))	
	}

	// error
	{
		VkPipelineShaderStageCreateInfo computeShaderStage = loadShader(getShadersPath() + "pbrtexture/error.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		VkPushConstantRange push_constant{};
		push_constant.size = sizeof(ErrorPushConstants);
		push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout(DescriptorType::errorPorj), 1);
		pipelineLayoutCreateInfo.pPushConstantRanges = &push_constant;
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &errorProjPipeline.pipelineLayout));

		VkComputePipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.stage = computeShaderStage;
		pipelineCreateInfo.layout = errorProjPipeline.pipelineLayout;
		VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &errorProjPipeline.pipeline));
	}

}

// Generate a BRDF integration map used as a look-up-table (stores roughness / NdotV)
void PBRTexture::generateBRDFLUT()
{
	vks::vksTools::generateBRDFLUT(*this);
}

// Generate an irradiance cube map from the environment cube map
void PBRTexture::generateIrradianceCube()
{
	vks::vksTools::generateIrradianceCube(*this);
}

// Prefilter environment cubemap
// See https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/
void PBRTexture::generatePrefilteredCube()
{
	vks::vksTools::generatePrefilteredCube(*this);
}

// Prepare and initialize uniform buffer containing shader uniforms
void PBRTexture::prepareUniformBuffers()
{
	// Object vertex shader uniform buffer
	VK_CHECK_RESULT(vulkanDevice->createBuffer( VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.scene, sizeof(uniformDataMatrices)));

	// Skybox vertex shader uniform buffer
	VK_CHECK_RESULT(vulkanDevice->createBuffer( VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.skybox, sizeof(uniformDataMatrices)));

	// Shared parameter uniform buffer
	VK_CHECK_RESULT(vulkanDevice->createBuffer( VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.params, sizeof(uniformDataMatrices)));

	// Map persistent
	VK_CHECK_RESULT(uniformBuffers.scene.map());
	VK_CHECK_RESULT(uniformBuffers.skybox.map());
	VK_CHECK_RESULT(uniformBuffers.params.map());

	updateUniformBuffers();
	updateParams();
}

void PBRTexture::updateUniformBuffers()
{
	// 3D object
	uniformDataMatrices.projection = camera.matrices.perspective;
	uniformDataMatrices.view = camera.matrices.view;
	uniformDataMatrices.model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	uniformDataMatrices.camPos = camera.position * -1.0f;
	memcpy(uniformBuffers.scene.mapped, &uniformDataMatrices, sizeof(vks::UniformDataMatrices));

	// Skybox
	uniformDataMatrices.model = glm::mat4(glm::mat3(camera.matrices.view));
	memcpy(uniformBuffers.skybox.mapped, &uniformDataMatrices, sizeof(vks::UniformDataMatrices));

	uboErrorMatrices.view = camera.matrices.view;
	uboErrorMatrices.proj = camera.matrices.perspective;
	uboErrorMatrices.camRight = camera.getRight();
	uboErrorMatrices.camUp = camera.getUp();
	memcpy(errorUniformBuffer.mapped, &uboErrorMatrices, sizeof(vks::UBOErrorMatrices));
}

void PBRTexture::updateParams()
{
	constexpr float p = 15.0f;
	uniformDataParams.lights[0] = glm::vec4(-p, -p * 0.5f, -p, 1.0f);
	uniformDataParams.lights[1] = glm::vec4(-p, -p * 0.5f, p, 1.0f);
	uniformDataParams.lights[2] = glm::vec4(p, -p * 0.5f, p, 1.0f);
	uniformDataParams.lights[3] = glm::vec4(p, -p * 0.5f, -p, 1.0f);

	memcpy(uniformBuffers.params.mapped, &uniformDataParams, sizeof(uniformDataParams));
}

void PBRTexture::prepare()
{
	initLogSystem();
	enabledDeviceExtensions.emplace_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
	
	VulkanExampleBase::prepare();
	loadAssets();
	generateBRDFLUT();
	generateIrradianceCube();
	generatePrefilteredCube();
	
	createCullingBuffers();
	createHizBuffer();
	createErrorProjectionBuffers();
	prepareUniformBuffers();
	setupDescriptors();
	preparePipelines();
	buildCommandBuffers();

	prepared = true;
}

void PBRTexture::buildCommandBuffers()
{	
	int workgroupX = 8;
	int workgroupY = 8;
	
	VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
	auto descMgr = VulkanDescriptorManager::getManager();

	VkClearValue clearValues[2];
	clearValues[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
	clearValues[1].depthStencil = {1.0f, 0};

	VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;
	
	for (size_t i = 0; i < drawCmdBuffers.size(); ++i)
	{
		// Set target frame buffer
		renderPassBeginInfo.framebuffer = frameBuffers[i];

		VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo))

		// culling和error的compute shader逻辑计算
		VkBufferMemoryBarrier bufferBarrier = {};
		bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferBarrier.buffer = projectedErrorBuffer.buffer;
		bufferBarrier.offset = 0;
		bufferBarrier.size = VK_WHOLE_SIZE;

		vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,0, nullptr, 1, &bufferBarrier, 0, nullptr);

		vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, errorProjPipeline.pipeline);
		errorPushConstants.numClusters = clusterInfos.size();
		errorPushConstants.screenSize = glm::vec2(width, height);
		vkCmdPushConstants(drawCmdBuffers[i], errorProjPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ErrorPushConstants), &errorPushConstants);

		// 启动error shader
		vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, errorProjPipeline.pipelineLayout, 0, 1, &descMgr->getSet(DescriptorType::errorPorj, 0), 0, 0);
		vkCmdDispatch(drawCmdBuffers[i], (errorPushConstants.numClusters + 63)/64, 1, 1);

		bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferBarrier.buffer = projectedErrorBuffer.buffer;
		bufferBarrier.offset = 0;
		bufferBarrier.size = VK_WHOLE_SIZE;
		vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,0, nullptr, 1, &bufferBarrier, 0, nullptr);
		
		// culling shader
		// 因为布局问题，进行转换
		VkImageMemoryBarrier imageMemBarrier = vks::initializers::imageMemoryBarrier();
		imageMemBarrier.image = textures.hizBuffer.image;
		imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageMemBarrier.subresourceRange.baseMipLevel = 0;
		imageMemBarrier.subresourceRange.levelCount = textures.hizBuffer.mipLevels;
		imageMemBarrier.subresourceRange.baseArrayLayer = 0;
		imageMemBarrier.subresourceRange.layerCount = 1;
		vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);

		vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, cullingPipeline.pipeline);
		cullingPushConstants.numClusters = clusterInfos.size();
		vkCmdPushConstants(drawCmdBuffers[i], cullingPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CullingPushConstants), &cullingPushConstants);
		vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, cullingPipeline.pipelineLayout, 0, 1, &descMgr->getSet(DescriptorType::culling, 0), 0, 0);
		vkCmdDispatch(drawCmdBuffers[i], (cullingPushConstants.numClusters + 63)/64, 1, 1);

		// 布局记得变回来
		imageMemBarrier.image = textures.hizBuffer.image;
		imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageMemBarrier.subresourceRange.baseMipLevel = 0;
		imageMemBarrier.subresourceRange.levelCount = textures.hizBuffer.mipLevels;
		imageMemBarrier.subresourceRange.baseArrayLayer = 0;
		imageMemBarrier.subresourceRange.layerCount = 1;
		vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);

		// draw indirect的处理
		bufferBarrier = {};
		bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		bufferBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferBarrier.buffer = drawIndexedIndirectBuffer.buffer;
		bufferBarrier.offset = 0;
		bufferBarrier.size = VK_WHOLE_SIZE;

		vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

		// culling 布局处理
		bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		bufferBarrier.dstAccessMask = VK_ACCESS_INDEX_READ_BIT;
		bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferBarrier.buffer = culledIndicesBuffer.buffer;
		bufferBarrier.offset = 0;
		bufferBarrier.size = VK_WHOLE_SIZE;
		vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
		
		// 渲染帧
		vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		VkViewport viewport = vks::initializers::viewport(static_cast<float>(width), static_cast<float>(height), 0.0f,
		                                                  1.0f);
		vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

		VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
		vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

		VkDeviceSize offsets[1] = {0};

		// Skybox
		if (displaySkybox)
		{
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
			                        &descMgr->getSet(DescriptorType::Scene, 4), 0, nullptr);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);
			models.skybox.draw(drawCmdBuffers[i]);
		}

		// Objects
		vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
		                        &descMgr->getSet(DescriptorType::Scene, 0), 0, nullptr);
		vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.pbr);
		// models.object.draw(drawCmdBuffers[i]);

		// 模型绘制部分的代码
		vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &scene.vertices.buffer, offsets);
		vkCmdBindIndexBuffer(drawCmdBuffers[i], culledIndicesBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexedIndirect(drawCmdBuffers[i], drawIndexedIndirectBuffer.buffer, 0, 1, 0);

		drawUI(drawCmdBuffers[i]);
		vkCmdEndRenderPass(drawCmdBuffers[i]);
		
		// depth copy 内存布局转换
		std::vector<VkImageMemoryBarrier> imageMemBarriers{1};
		imageMemBarriers[0] = vks::initializers::imageMemoryBarrier();
		imageMemBarriers[0].image = depthStencil.image;
		imageMemBarriers[0].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		imageMemBarriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageMemBarriers[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		imageMemBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imageMemBarriers[0].subresourceRange = vks::vksTools::genDepthSubresourceRange();
		
		vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarriers[0]);
		
		vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, depthCopyPipeline.pipeline);
		vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, depthCopyPipeline.pipelineLayout, 0, 1, &descMgr->getSet(DescriptorType::depthCopy, 0), 0, 0);
		vkCmdDispatch(drawCmdBuffers[i], (width + workgroupX - 1) / workgroupX, (height + workgroupY - 1) / workgroupY, 1);
		
		// 恢复原样，方便后续操作
		imageMemBarriers[0] = vks::initializers::imageMemoryBarrier();
		imageMemBarriers[0].image = depthStencil.image;
		imageMemBarriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageMemBarriers[0].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		imageMemBarriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imageMemBarriers[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		imageMemBarriers[0].subresourceRange = vks::vksTools::genDepthSubresourceRange();
		
		vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 0, 0, 0, 0, imageMemBarriers.size(), imageMemBarriers.data());
		
		// 开始为hiz buffer添加image barrier
		vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, hizComputePipeline.pipeline);
		for (int j = 0; j < textures.hizBuffer.mipLevels - 1; j++)
		{
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, hizComputePipeline.pipelineLayout, 0, 1, &descMgr->getSet(DescriptorType::hiz, j), 0, nullptr);
			vkCmdDispatch(drawCmdBuffers[i], (width + workgroupX - 1)/workgroupX, (height + workgroupY - 1) / workgroupY, 1);
			
			VkImageMemoryBarrier imageBarrier = vks::initializers::imageMemoryBarrier();
			imageBarrier.image = textures.hizBuffer.image;
			imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBarrier.subresourceRange.baseMipLevel = j+1;
			imageBarrier.subresourceRange.levelCount = 1;
			imageBarrier.subresourceRange.baseArrayLayer = 0;
			imageBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,nullptr, 1, &imageBarrier);
		}
		
		if (false){ // depth copy的代码绘制
			VkImageMemoryBarrier imageMemBarrier = vks::initializers::imageMemoryBarrier();
			imageMemBarrier.image = textures.hizBuffer.image;
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemBarrier.subresourceRange.baseMipLevel = 0;
			imageMemBarrier.subresourceRange.levelCount = textures.hizBuffer.mipLevels;
			imageMemBarrier.subresourceRange.baseArrayLayer = 0;
			imageMemBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);
			// hiz buffer
			
			// VK_SUBPASS_CONTENTS_INLINE 直接进入内联模式，不整花里胡哨
			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdSetViewport(drawCmdBuffers[i], 0,1, &viewport);
			vkCmdSetScissor(drawCmdBuffers[i], 0,1, &scissor);
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, debugQuadPipeline.pipelineLayout, 0, 1, &descMgr->getSet(DescriptorType::debugQuad, 0), 0, nullptr);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, debugQuadPipeline.pipeline);
			vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);
			vkCmdEndRenderPass(drawCmdBuffers[i]);
			
			imageMemBarrier.image = textures.hizBuffer.image;
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemBarrier.subresourceRange.baseMipLevel = 0;
			imageMemBarrier.subresourceRange.levelCount = textures.hizBuffer.mipLevels;
			imageMemBarrier.subresourceRange.baseArrayLayer = 0;
			imageMemBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);
		}

		VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
	}
}

void PBRTexture::render()
{
	if (!prepared) return;

	prepareFrame();

	if (drawIndexedIndirectBuffer.mapped)
	{
		drawIndexedIndirect.indexCount = 0;
		memcpy(drawIndexedIndirectBuffer.mapped, &drawIndexedIndirect, sizeof(vks::DrawIndexedIndirect));
		drawIndexedIndirectBuffer.flush();
		vkDeviceWaitIdle(device);
	}
	
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
	submitFrame();

	// culling是永久更新的，所以每帧重新绘制
	uboCullingMatrices.lastView = camera.matrices.view;
	uboCullingMatrices.lastProj = camera.matrices.perspective;
	memcpy(cullingUniformBuffer.mapped, &uboCullingMatrices, sizeof(vks::UBOCullingMatrices));
	cullingUniformBuffer.flush();

	if (camera.updated)
	{
		updateUniformBuffers();
	}
}

void PBRTexture::viewChanged()
{
	updateUniformBuffers();
}

void PBRTexture::OnUpdateUIOverlay(vks::UIOverlay* overlay)
{
	if (overlay->header("Settings"))
	{
		if (overlay->inputFloat("Exposure", &uniformDataParams.exposure, 0.1f, 2))
		{
			updateParams();
		}
		if (overlay->inputFloat("Gamma", &uniformDataParams.gamma, 0.1f, 2))
		{
			updateParams();
		}
		if (overlay->checkBox("Skybox", &displaySkybox))
		{
			buildCommandBuffers();
		}
	}
}

void PBRTexture::createHizBuffer()
{
	uint32_t mipmipLevels = std::floor(std::log2(std::max(width, height))) + 1;
	textures.hizBuffer.mipLevels = mipmipLevels;

	VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = VK_FORMAT_R32_SFLOAT;
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = mipmipLevels;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	// 采样mipmap，也会作为起点，同时storage传给下一个
	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &textures.hizBuffer.image));

	VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, textures.hizBuffer.image, &memRequirements);
	memAlloc.allocationSize = memRequirements.size;
	memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &textures.hizBuffer.deviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(device, textures.hizBuffer.image, textures.hizBuffer.deviceMemory, 0));

	VkImageViewCreateInfo viewCreateInfo = vks::initializers::imageViewCreateInfo();
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = VK_FORMAT_R32_SFLOAT;
	viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewCreateInfo.subresourceRange.levelCount = mipmipLevels;
	viewCreateInfo.subresourceRange.layerCount = 1;
	viewCreateInfo.image = textures.hizBuffer.image;
	VK_CHECK_RESULT(vkCreateImageView(device, &viewCreateInfo, nullptr, &textures.hizBuffer.view));

	// 涉及采样，所以要单独再开一个采样器
	VkSamplerCreateInfo samplerCreateInfo = vks::initializers::samplerCreateInfo();
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	// hiz需要保守剔除
	samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
	// 需要确保shader里面进行了multi sample
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerCreateInfo.maxLod = mipmipLevels;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	// 关闭各向异性
	samplerCreateInfo.anisotropyEnable = VK_FALSE;
	samplerCreateInfo.maxAnisotropy = 1.0f;

	VK_CHECK_RESULT(vkCreateSampler(device, &samplerCreateInfo, nullptr, &textures.hizBuffer.sampler));

	textures.hizBuffer.descriptor.imageView = textures.hizBuffer.view;
	textures.hizBuffer.descriptor.sampler = textures.hizBuffer.sampler;
	textures.hizBuffer.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	textures.hizBuffer.device = vulkanDevice;

	// 开始创建每个子一级别的level
	for (int i = 0; i < mipmipLevels; ++i)
	{
		VkImageView mipView;
		VkImageViewCreateInfo mipViewCreateInfo = vks::initializers::imageViewCreateInfo();
		mipViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		mipViewCreateInfo.format = VK_FORMAT_R32_SFLOAT;
		mipViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		mipViewCreateInfo.subresourceRange.levelCount = 1;
		mipViewCreateInfo.subresourceRange.layerCount = 1;
		mipViewCreateInfo.subresourceRange.baseMipLevel = i;
		mipViewCreateInfo.image = textures.hizBuffer.image;

		VK_CHECK_RESULT(vkCreateImageView(device, &mipViewCreateInfo, nullptr, &mipView));
		hizImageViews.emplace_back(mipView);
	}

	//修改imagelayuout
	VkCommandBuffer cmdBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	// 决定update imagelayout的粒度
	VkImageSubresourceRange subResourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .layerCount = 1,};
	subResourceRange.levelCount = textures.hizBuffer.mipLevels;

	vks::tools::setImageLayout(cmdBuffer, textures.hizBuffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, subResourceRange);

	vulkanDevice->flushCommandBuffer(cmdBuffer, queue, true);
	vkDeviceWaitIdle(device);
}

void PBRTexture::setupDepthStencil()
{
	VkImageCreateInfo imageCI{};
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	imageCI.format = depthFormat;
	imageCI.extent = {width, height, 1};
	imageCI.mipLevels = 1;
	imageCI.arrayLayers = 1;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	// 加一个会被shader使用的flag 
	// 在depth copy里面需要一个额外的 VK_IMAGE_USAGE_STORAGE_BIT 但是直接添加storage bit会有assert，因为
	// 这里使用的format是 VK_FORMAT_D32_SFLOAT_S8_UINT 不支持storage_bit
	// 最后决定采用的处理方法是在command阶段进行一个布局转换
	imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &depthStencil.image));
	VkMemoryRequirements memReqs{};
	vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);

	VkMemoryAllocateInfo memAllloc{};
	memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllloc.allocationSize = memReqs.size;
	memAllloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memAllloc, nullptr, &depthStencil.mem));
	VK_CHECK_RESULT(vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0));

	VkImageViewCreateInfo imageViewCI{};
	imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCI.image = depthStencil.image;
	imageViewCI.format = depthFormat;
	imageViewCI.subresourceRange.baseMipLevel = 0;
	imageViewCI.subresourceRange.levelCount = 1;
	imageViewCI.subresourceRange.baseArrayLayer = 0;
	imageViewCI.subresourceRange.layerCount = 1;
	imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

	// 不能同时设置depth和stencil 
	// Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
	// if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
	// 	imageViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	// }
	VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &depthStencil.view));

	VkSamplerCreateInfo samplerCreateInfo = vks::initializers::samplerCreateInfo();
	samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK_RESULT(vkCreateSampler(device, &samplerCreateInfo, nullptr, &depthStencilSampler));

	VkCommandBuffer cmdBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	VkImageSubresourceRange subResourceRange = vks::vksTools::genDepthSubresourceRange();

	vks::tools::setImageLayout(cmdBuffer, depthStencil.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, subResourceRange);
	vulkanDevice->flushCommandBuffer(cmdBuffer, queue, true);

	vkDeviceWaitIdle(device);
}

void PBRTexture::createCullingBuffers()
{
	// 创建剔除用的buffer
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, models.object.indices.count*sizeof(uint32_t), &culledIndicesBuffer.buffer, &culledIndicesBuffer.memory, nullptr))
	
	for (auto& clusterInfo : scene.clusterInfo)
	{
		clusterInfos.emplace_back(clusterInfo);
	}
	
	vks::vksTools::createStagingBuffer(*this, 0, clusterInfos.size()*sizeof(Nanite::ClusterInfo), clusterInfos.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT| VK_BUFFER_USAGE_INDEX_BUFFER_BIT, clustersInfoBuffer);
	
	// 剔除用的uniform buffer
	uboCullingMatrices.model = glm::mat4(1.0f);
	uboCullingMatrices.lastView = camera.matrices.view;
	uboCullingMatrices.lastProj = camera.matrices.perspective;
	
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(uboCullingMatrices), &cullingUniformBuffer.buffer, &cullingUniformBuffer.memory, &uboCullingMatrices));
	cullingUniformBuffer.device = device;
	VK_CHECK_RESULT(cullingUniformBuffer.map());
	
	drawIndexedIndirect.firstIndex = 0;
	drawIndexedIndirect.firstInstance = 0;
	drawIndexedIndirect.indexCount = models.object.indexBuffer.size();
	drawIndexedIndirect.instanceCount = 1;
	drawIndexedIndirect.vertexOffset = 0;
	
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(drawIndexedIndirect), &drawIndexedIndirectBuffer.buffer, &drawIndexedIndirectBuffer.memory, &drawIndexedIndirect));
	drawIndexedIndirectBuffer.device = device;
	VK_CHECK_RESULT(drawIndexedIndirectBuffer.map());
}

void PBRTexture::createErrorProjectionBuffers()
{
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scene.errorInfo.size()*sizeof(glm::vec2), &projectedErrorBuffer.buffer, &projectedErrorBuffer.memory, nullptr))

	for (auto & errorInfo : scene.errorInfo)
	{
		errorInfos.emplace_back(errorInfo);
	}
	vks::vksTools::createStagingBuffer(*this, 0, errorInfos.size()*sizeof(Nanite::ErrorInfo), errorInfos.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, errorInfoBuffer);

	// uniform init
	uboErrorMatrices.view = camera.matrices.view;
	uboErrorMatrices.proj = camera.matrices.perspective;
	uboErrorMatrices.camRight = camera.getRight();
	uboErrorMatrices.camUp = camera.getUp();
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		sizeof(vks::UBOErrorMatrices),
		&errorUniformBuffer.buffer,
		&errorUniformBuffer.memory,
		&uboErrorMatrices));

	errorUniformBuffer.device = device;
	VK_CHECK_RESULT(errorUniformBuffer.map());
}

void PBRTexture::initLogSystem()
{
	auto& Logger = Log::Logger::Instance();
	Logger.SetLevel(Log::Level::Trace);
	Logger.EnableColor(true);
	Logger.SetLogFile("cyVulkanNanite.log");
	
	// 如何使用log系统
	
}

void PBRTexture::createNaniteScene()
{
	scene.naniteMeshes.emplace_back(naniteMesh);
	modelMats.clear();
	
	for (int i = 0; i <= 0; i++)
	{
		for (int j = 0; j <= 0; j++) 
		{
			auto modelMat = glm::translate(glm::mat4(1.0f), glm::vec3(i * 3, 1.2f, j * 3));
			auto instance = Nanite::NaniteInstance(&naniteMesh, modelMat);
			modelMats.emplace_back(modelMat);
			scene.naniteObjects.emplace_back(instance);
		}
	}
	
	scene.createVertexIndexBuffer(*this);
	scene.createClusterInfos();
}
