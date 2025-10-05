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


PBRTexture::PBRTexture() : VulkanExampleBase(true)
{
	title = "Textured PBR with IBL";
	camera.type = Camera::CameraType::firstperson;
	camera.movementSpeed = 4.0f;
	camera.setPerspective(60.0f, static_cast<float>(width) / static_cast<float>(height), 0.1f, 256.0f);
	camera.rotationSpeed = 0.25f;
	camera.setRotation({-7.75f, 150.25f, 0.0f});
	camera.setPosition({0.7f, 0.1f, 1.7f});
}

PBRTexture::~PBRTexture()
{
	if (!device) return;

	// 销毁Pipeline
	vkDestroyPipeline(device, pipelines.skybox, nullptr);
	vkDestroyPipeline(device, pipelines.pbr, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

	// 销毁描述符管理器
	if (auto descMgr = VulkanDescriptorManager::getManager())
	{
		descMgr->destroy();
	}

	// 销毁资源
	textures.destroy();
	uniformBuffers.destroy();

	// 销毁HIZ ImageView
	for (auto& imageView : hizImageViews)
	{
		vkDestroyImageView(device, imageView, nullptr);
	}
	hizImageViews.clear();

	// 销毁Compute Pipeline
	hizComputePipeline.destroy(device);
	depthCopyPipeline.destroy(device);
	debugQuadPipeline.destroy(device);
}

void PBRTexture::getEnabledFeatures()
{
	if (deviceFeatures.samplerAnisotropy) {
		enabledFeatures.samplerAnisotropy = VK_TRUE;
	}
	if (deviceFeatures.fillModeNonSolid) {
		enabledFeatures.fillModeNonSolid = VK_TRUE;
	}
	if (deviceFeatures.geometryShader) {
		enabledFeatures.geometryShader = VK_TRUE;
	}
	if (deviceFeatures.shaderInt64) {
		enabledFeatures.shaderInt64 = VK_TRUE;
	}
	if (deviceFeatures.tessellationShader) {
		enabledFeatures.tessellationShader = VK_TRUE;
	}
	if (deviceFeatures.fragmentStoresAndAtomics) {
		enabledFeatures.fragmentStoresAndAtomics = VK_TRUE;
	}
}

void PBRTexture::getEnabledInstanceExtensions()
{
	enabledInstanceExtensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
}

void PBRTexture::getEnabledDeviceExtensions()
{
	enabledDeviceExtensions.emplace_back(VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME);
	enabledDeviceExtensions.emplace_back(VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME);
		
	imageAtomicInt64Feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT;
	imageAtomicInt64Feature.shaderImageInt64Atomics = VK_TRUE;
	deviceCreatepNextChain = &imageAtomicInt64Feature;
}

void PBRTexture::loadAssets()
{
	constexpr uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;

	const std::string assetPath = getAssetPath();

	models.skybox.loadFromFile(assetPath + "models/cube.gltf", vulkanDevice, queue, glTFLoadingFlags);
	models.object.loadFromFile(assetPath + "models/bunny.gltf", vulkanDevice, queue, glTFLoadingFlags);

	// Nanite mesh初始化
	naniteMesh.setModelPath((assetPath + "models/bunny/").c_str());
	naniteMesh.loadvkglTFModel(models.object);
	naniteMesh.initNaniteInfo(assetPath + "models/bunny.gltf", true);

	for (auto& lodMesh : naniteMesh.meshes)
	{
		lodMesh.initUniqueVertexBuffer();
		lodMesh.initVertexBuffer();
		lodMesh.createVertexBuffer(*this);
	}

	createNaniteScene();

	// 加载纹理
	textures.environmentCube.loadFromFile(assetPath + "textures/hdr/gcanyon_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice, queue);
	textures.albedoMap.loadFromFile(assetPath + "models/cerberus/albedo.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
	textures.normalMap.loadFromFile(assetPath + "models/cerberus/normal.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
	textures.aoMap.loadFromFile(assetPath + "models/cerberus/ao.ktx", VK_FORMAT_R8_UNORM, vulkanDevice, queue);
	textures.metallicMap.loadFromFile(assetPath + "models/cerberus/metallic.ktx", VK_FORMAT_R8_UNORM, vulkanDevice, queue);
	textures.roughnessMap.loadFromFile(assetPath + "models/cerberus/roughness.ktx", VK_FORMAT_R8_UNORM, vulkanDevice, queue);
}

void PBRTexture::setupDescriptors()
{
	vks::vksTools::setPbrDescriptor(*this);
}

VkBufferMemoryBarrier PBRTexture::createBufferBarrier(VkBuffer buffer, VkAccessFlags srcAccess, VkAccessFlags dstAccess)
{
	VkBufferMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = srcAccess;
	barrier.dstAccessMask = dstAccess;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = buffer;
	barrier.offset = 0;
	barrier.size = VK_WHOLE_SIZE;
	return barrier;
}

VkImageMemoryBarrier PBRTexture::createImageBarrier(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess, const VkImageSubresourceRange& subresourceRange)
{
	VkImageMemoryBarrier barrier = vks::initializers::imageMemoryBarrier();
	barrier.image = image;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcAccessMask = srcAccess;
	barrier.dstAccessMask = dstAccess;
	barrier.subresourceRange = subresourceRange;
	return barrier;
}

void PBRTexture::createGraphicsPipelines()
{
	auto descManager = VulkanDescriptorManager::getManager();
	const std::string shaderPath = getShadersPath() + "pbrtexture/";
	
	// 通用状态
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
	VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
	VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
	std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
	std::array<VkPipelineShaderStageCreateInfo, 3> shaderStages;
	std::array<VkPipelineShaderStageCreateInfo, 3> pbrShaderStages;
	VkPushConstantRange push_constant = {.size = sizeof(RenderingPushConstants)};
	push_constant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	
	// Pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout(DescriptorType::Scene), 1);
	pipelineLayoutCI.pushConstantRangeCount = 1;
	pipelineLayoutCI.pPushConstantRanges = &push_constant;
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

	// Pipeline创建信息
	VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
	pipelineCI.pInputAssemblyState = &inputAssemblyState;
	pipelineCI.pRasterizationState = &rasterizationState;
	pipelineCI.pColorBlendState = &colorBlendState;
	pipelineCI.pMultisampleState = &multisampleState;
	pipelineCI.pViewportState = &viewportState;
	pipelineCI.pDepthStencilState = &depthStencilState;
	pipelineCI.pDynamicState = &dynamicState;
	pipelineCI.stageCount = 2;
	pipelineCI.pStages = shaderStages.data();
	pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState(
	{vkglTF::VertexComponent::Position, 
	vkglTF::VertexComponent::Normal, 
	vkglTF::VertexComponent::UV,
	 vkglTF::VertexComponent::Tangent, 
	 vkglTF::VertexComponent::Joint0,
	  vkglTF::VertexComponent::Weight0
	});

	// Skybox pipeline
	rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
	shaderStages[0] = loadShader(shaderPath + std::string(vks::ShaderName::skyboxVert), VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(shaderPath + std::string(vks::ShaderName::skyboxFrag), VK_SHADER_STAGE_FRAGMENT_BIT);
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.skybox));

	// PBR pipeline
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthTestEnable = VK_TRUE;
	shaderStages[0] = loadShader(shaderPath + std::string(vks::ShaderName::pbrtextureVert), VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(shaderPath + std::string(vks::ShaderName::pbrtextureFrag), VK_SHADER_STAGE_FRAGMENT_BIT);
	shaderStages[2] = loadShader(shaderPath + std::string(vks::ShaderName::pbrtextureGeom), VK_SHADER_STAGE_GEOMETRY_BIT);
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.pbr));
	
	pipelineCI.stageCount = 2;
	
	// hardware rasterize pipeline
	pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout(DescriptorType::hwRast), 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &hwrastPipeline.pipelineLayout));
	pipelineCI.layout = hwrastPipeline.pipelineLayout;
	pipelineCI.renderPass = hwRasterizeRenderPass;
	
	shaderStages[0] = loadShader(shaderPath + std::string(vks::ShaderName::hwrasterizeVert), VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(shaderPath + std::string(vks::ShaderName::hwrasterizeFrag), VK_SHADER_STAGE_FRAGMENT_BIT);
	shaderStages[2] = loadShader(shaderPath + std::string(vks::ShaderName::hwrasterizeGeom), VK_SHADER_STAGE_GEOMETRY_BIT);
	pipelineCI.pStages = shaderStages.data();
	pipelineCI.stageCount = 3;
	rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &hwrastPipeline.pipeline));
	
	// Debug quad pipeline
	pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout(DescriptorType::debugQuad), 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &debugQuadPipeline.pipelineLayout));

	pipelineCI.layout = debugQuadPipeline.pipelineLayout;
	pipelineCI.renderPass = renderPass;
	depthStencilState.depthWriteEnable = VK_FALSE;
	depthStencilState.depthTestEnable = VK_FALSE;
	rasterizationState.cullMode = VK_CULL_MODE_NONE;
	VkPipelineVertexInputStateCreateInfo emptyVertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
	pipelineCI.pVertexInputState = &emptyVertexInputState;
	pipelineCI.stageCount = 2;
	shaderStages[0] = loadShader(shaderPath + std::string(vks::ShaderName::debugQuadVert), VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(shaderPath + std::string(vks::ShaderName::debugQuadFrag), VK_SHADER_STAGE_FRAGMENT_BIT);
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &debugQuadPipeline.pipeline));
	
	// shading pipeline
	push_constant.size = sizeof(RenderingPushConstants);
	push_constant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout(DescriptorType::shading), 1);
	pipelineLayoutCI.pushConstantRangeCount = 1;
	pipelineLayoutCI.pPushConstantRanges = &push_constant;
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &shadingPipeline.pipelineLayout));
	
	pipelineCI.layout = shadingPipeline.pipelineLayout;
	pipelineCI.renderPass = renderPass;	
	shaderStages[0] = loadShader(shaderPath + std::string(vks::ShaderName::shadingVert), VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(shaderPath + std::string(vks::ShaderName::shadingFrag), VK_SHADER_STAGE_FRAGMENT_BIT);
	pipelineCI.stageCount = 2;
	pipelineCI.pStages = shaderStages.data();
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &shadingPipeline.pipeline));
	pipelineLayoutCI.pushConstantRangeCount = 0;
}

void PBRTexture::createComputePipelines()
{
	auto descManager = VulkanDescriptorManager::getManager();
	const std::string shaderPath = getShadersPath() + "pbrtexture/";

	auto createComputePipeline = [&](const std::string& shaderName, DescriptorType descType, Pipeline& pipeline, const VkPushConstantRange* pushConstant = nullptr)
	{
		VkPipelineShaderStageCreateInfo stage = loadShader(shaderPath + shaderName, VK_SHADER_STAGE_COMPUTE_BIT);
		VkPipelineLayoutCreateInfo layoutCI = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout(descType), 1);
		if (pushConstant)
		{
			layoutCI.pPushConstantRanges = pushConstant;
			layoutCI.pushConstantRangeCount = 1;
		}
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &layoutCI, nullptr, &pipeline.pipelineLayout));

		VkComputePipelineCreateInfo pipelineCI = vks::initializers::computePipelineCreateInfo(pipeline.pipelineLayout);
		pipelineCI.stage = stage;
		VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline.pipeline));
	};
	
	// hiz
	createComputePipeline("genHiz.comp.spv", DescriptorType::hiz, hizComputePipeline);
	// clear image
	createComputePipeline("clearImage.comp.spv", DescriptorType::clearImage, clearImagePipeline);
	// software rasterize pipeline
	createComputePipeline("swrasterize.comp.spv", DescriptorType::swRast, swComputePipeline);
	// depth copy
	createComputePipeline("depthCopy.comp.spv", DescriptorType::depthCopy, depthCopyPipeline);
	// culling
	VkPushConstantRange cullingPush{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CullingPushConstants)};
	createComputePipeline("culling.comp.spv", DescriptorType::culling, cullingPipeline, &cullingPush);
	// error
	VkPushConstantRange errorPush{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ErrorPushConstants)};
	createComputePipeline("error.comp.spv", DescriptorType::errorPorj, errorProjPipeline, &errorPush);
	// merge
	VkPushConstantRange mergePush{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(renderPushConstants)};
	createComputePipeline("mergeRast.comp.spv", DescriptorType::mergeRast, mergeRastPipeline, &mergePush);
}

void PBRTexture::setupRenderPass()
{
	// VulkanExampleBase::setupRenderPass();
	
	std::array<VkAttachmentDescription, 2> attachments = {};
	// Color attachment
	attachments[0].format = swapChain.colorFormat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	// Depth attachment
	attachments[1].format = depthFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pDepthStencilAttachment = &depthReference;
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pInputAttachments = nullptr;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = nullptr;
	subpassDescription.pResolveAttachments = nullptr;

	// Subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	dependencies[0].dependencyFlags = 0;

	dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].dstSubpass = 0;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].srcAccessMask = 0;
	dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	dependencies[1].dependencyFlags = 0;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));

	// Color attachment
	attachments[0].format = VK_FORMAT_R32_UINT;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	// Depth attachment
	attachments[1].format = VK_FORMAT_D32_SFLOAT;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pDepthStencilAttachment = &depthReference;
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pInputAttachments = nullptr;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = nullptr;
	subpassDescription.pResolveAttachments = nullptr;

	// Subpass dependencies for layout transitions

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	dependencies[0].dependencyFlags = 0;

	dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].dstSubpass = 0;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].srcAccessMask = 0;
	dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	dependencies[1].dependencyFlags = 0;

	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &hwRasterizeRenderPass));

}

void PBRTexture::preparePipelines()
{
	createGraphicsPipelines();
	createComputePipelines();
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
	constexpr VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	constexpr VkMemoryPropertyFlags memProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	VK_CHECK_RESULT(vulkanDevice->createBuffer(usage, memProps, &uniformBuffers.scene, sizeof(uniformDataMatrices)));
	VK_CHECK_RESULT(vulkanDevice->createBuffer(usage, memProps, &uniformBuffers.skybox, sizeof(uniformDataMatrices)));
	VK_CHECK_RESULT(vulkanDevice->createBuffer(usage, memProps, &uniformBuffers.params, sizeof(uniformDataParams)));

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
	
	// error
	uboErrorMatrices.view = camera.matrices.view;
	uboErrorMatrices.proj = camera.matrices.perspective;
	uboErrorMatrices.camRight = camera.getRight();
	uboErrorMatrices.camUp = camera.getUp();
	memcpy(errorUniformBuffer.mapped, &uboErrorMatrices, sizeof(uboErrorMatrices));
	errorUniformBuffer.flush();
	
	uboShading.invView = glm::inverse(camera.matrices.view);
	uboShading.invProj = glm::inverse(camera.matrices.perspective);
	uboShading.camPos = camera.position*-1.0f;
	memcpy(uniformBuffers.shadingMats.mapped, &uboShading, sizeof(vks::UBOShading));
	uniformBuffers.shadingMats.flush();
	
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
	enabledDeviceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	enabledDeviceExtensions.emplace_back(VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME);

	VulkanExampleBase::prepare();
	loadAssets();
	createRasterizeBuffer();
	
	generateBRDFLUT();
	generateIrradianceCube();
	generatePrefilteredCube();

	createCullingBuffers();
	createErrorProjectionBuffers();
	createHizBuffer();
	createModelMatsBuffer();
	createHWRasterizeFrameBuffer();
	prepareUniformBuffers();
	setupDescriptors();
	preparePipelines();
	buildCommandBuffers();

	prepared = true;
}

void PBRTexture::recordComputeCommands(VkCommandBuffer cmdBuffer, size_t /*frameIndex*/)
{
	auto descMgr = VulkanDescriptorManager::getManager();

	// Error projection compute
	// error proj -> culling
	auto barrier = createBufferBarrier(projectedErrorBuffer.buffer, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
	
	// space error compute
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, errorProjPipeline.pipeline);
	errorPushConstants.numClusters = static_cast<int>(clusterInfos.size());
	errorPushConstants.screenSize = glm::vec2(width, height);
	vkCmdPushConstants(cmdBuffer, errorProjPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ErrorPushConstants), &errorPushConstants);
	vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, errorProjPipeline.pipelineLayout, 0, 1, &descMgr->getSet(DescriptorType::errorPorj, 0), 0, nullptr);
	vkCmdDispatch(cmdBuffer, (errorPushConstants.numClusters + DISPATCH_GROUP_SIZE - 1) / DISPATCH_GROUP_SIZE, 1, 1);
	
	// culling -> error proj
	barrier = createBufferBarrier(projectedErrorBuffer.buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);

	// Culling compute 
	// 先进行一下HIZ布局转换
	VkImageSubresourceRange hizRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, textures.hizBuffer.mipLevels, 0, 1};
	auto imgBarrier = createImageBarrier(textures.hizBuffer.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, hizRange);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgBarrier);
	
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cullingPipeline.pipeline);
	
	cullingPushConstants.numClusters = static_cast<int>(clusterInfos.size());
	cullingPushConstants.threshold = thresholdInt/thresholdIntDiv;
	vkCmdPushConstants(cmdBuffer, cullingPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CullingPushConstants), &cullingPushConstants);
	vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cullingPipeline.pipelineLayout, 0, 1, &descMgr->getSet(DescriptorType::culling, 0), 0, nullptr);
	vkCmdDispatch(cmdBuffer, (cullingPushConstants.numClusters + DISPATCH_GROUP_SIZE - 1) / DISPATCH_GROUP_SIZE, 1, 1);

	// 恢复HIZ布局
	imgBarrier = createImageBarrier(textures.hizBuffer.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, hizRange);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgBarrier);

	// Indirect and hw draw buffer barrier 
	barrier = createBufferBarrier(drawIndexedIndirectBuffer.buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);

	barrier = createBufferBarrier(hwRIndicesBuffer.buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDEX_READ_BIT);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
	
	barrier = createBufferBarrier(hwRIDBuffer.buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
	
	// soft ware rasterize
	// clear image的barrier
	barrier = createBufferBarrier(swNumVerticesBuffer.buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
	
	barrier = createBufferBarrier(swRIndicesBuffer.buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
	
	barrier = createBufferBarrier(swRIDBuffer.buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
	
	// clear image
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, clearImagePipeline.pipeline);
	vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, clearImagePipeline.pipelineLayout, 0, 1, &descMgr->getSet(DescriptorType::clearImage, 0), 0, 0);
	vkCmdDispatch(cmdBuffer, (width + WORKGROUP_SIZE_X - 1) / WORKGROUP_SIZE_X, (height + WORKGROUP_SIZE_Y - 1) / WORKGROUP_SIZE_Y, 1);
	
	// sw rast
	barrier = createBufferBarrier(swIndirectDispatchBuffer.buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0,0,nullptr,1,&barrier,0,nullptr);
	
	VkImageSubresourceRange swRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	imgBarrier = createImageBarrier(SWRasterizeBuffer.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, swRange);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,0,0,0,0,01,&imgBarrier);
	
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, swComputePipeline.pipeline);
	vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, swComputePipeline.pipelineLayout, 0, 1, &descMgr->getSet(DescriptorType::swRast, 0), 0, 0);
	vkCmdDispatchIndirect(cmdBuffer, swIndirectDispatchBuffer.buffer, 0);
	
	imgBarrier = createImageBarrier(SWRasterizeBuffer.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, swRange);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imgBarrier);
}

void PBRTexture::recordHardwareRasterize(VkCommandBuffer cmdBuffer, size_t frameIndex, const VkRenderPassBeginInfo& rpBeginInfo)
{
	auto descMgr = VulkanDescriptorManager::getManager();
	
	// 为了可以蹭到硬件的优化，通过render pass去获取hard ware rasterize的相关数据
	VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
	VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
	VkClearValue clearValues1[2] = {};
	clearValues1[0].color.uint32[0] = UINT32_MAX;
	clearValues1[0].color.uint32[1] = UINT32_MAX;
	clearValues1[0].color.uint32[2] = UINT32_MAX;
	clearValues1[0].color.uint32[3] = UINT32_MAX;
	clearValues1[1].depthStencil = { 1.0f, 0 };
	VkRenderPassBeginInfo renderPassBeginInfo1 = vks::initializers::renderPassBeginInfo();
	renderPassBeginInfo1.framebuffer = HWRasterizeFrameBuffer;
	renderPassBeginInfo1.renderPass = hwRasterizeRenderPass;
	renderPassBeginInfo1.renderArea.offset.x = 0;
	renderPassBeginInfo1.renderArea.offset.y = 0;
	renderPassBeginInfo1.renderArea.extent.width = width;
	renderPassBeginInfo1.renderArea.extent.height = height;
	renderPassBeginInfo1.clearValueCount = 2;
	renderPassBeginInfo1.pClearValues = clearValues1;
	
	vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo1, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hwrastPipeline.pipeline);
	vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
	vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
	vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hwrastPipeline.pipelineLayout, 0, 1, &descMgr->getSet(DescriptorType::hwRast, 0), 0, NULL);
	vkCmdBindIndexBuffer(cmdBuffer, hwRIndicesBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &scene.vertices.buffer, offsets);
	vkCmdDrawIndexedIndirect(cmdBuffer, drawIndexedIndirectBuffer.buffer, 0, 1, 0);
	vkCmdEndRenderPass(cmdBuffer);
	
	VkBufferMemoryBarrier barrier = createBufferBarrier(hwRIndicesBuffer.buffer, VK_ACCESS_INDEX_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
	
	barrier = createBufferBarrier(hwRIndicesBuffer.buffer, VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
	
	VkImageSubresourceRange imageSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
	VkImageMemoryBarrier imgBarrier = createImageBarrier(HWRasterizeBuffer.image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, imageSubresource);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imgBarrier);
	
	imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imgBarrier = createImageBarrier(HWRVisBuffer.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT, imageSubresource);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imgBarrier);

}

void PBRTexture::recordMerge(VkCommandBuffer cmdBuffer, size_t frameIndex)
{	
	auto descMgr = VulkanDescriptorManager::getManager();
	
	// merge
	vkCmdPushConstants(cmdBuffer, mergeRastPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(RenderingPushConstants), &renderPushConstants);
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mergeRastPipeline.pipeline);
	vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mergeRastPipeline.pipelineLayout, 0, 1, &descMgr->getSet(DescriptorType::mergeRast, 0), 0, NULL);
	vkCmdDispatch(cmdBuffer, (width + WORKGROUP_SIZE_X - 1) / WORKGROUP_SIZE_X, (height + WORKGROUP_SIZE_Y - 1) / WORKGROUP_SIZE_Y, 1);
	
	VkImageSubresourceRange imageSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
	VkImageMemoryBarrier imgBarrier = createImageBarrier(HWRasterizeBuffer.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, imageSubresource);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 0, 0, 0, 0, 1, &imgBarrier);
	
	imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imgBarrier = createImageBarrier(HWRVisBuffer.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, imageSubresource);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, 0, 0, 0, 1, &imgBarrier);

	imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;	
	imgBarrier = createImageBarrier(finalZBuffer.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, imageSubresource);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imgBarrier);
	
	imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imgBarrier = createImageBarrier(finalVisBuffer.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, imageSubresource);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imgBarrier);
}

void PBRTexture::buildCommandBuffers()
{
	VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
	
	VkClearValue clearValues[2];
	clearValues[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
	clearValues[1].depthStencil = {1.0f, 0};

	VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.renderArea = {{0, 0}, {width, height}};
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	for (size_t i = 0; i < drawCmdBuffers.size(); ++i)
	{
		renderPassBeginInfo.framebuffer = frameBuffers[i];
		VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

		recordComputeCommands(drawCmdBuffers[i], i);
		recordHardwareRasterize(drawCmdBuffers[i], i, renderPassBeginInfo);
		recordMerge(drawCmdBuffers[i], i);
		recordRenderPassCommands(drawCmdBuffers[i], renderPassBeginInfo);
		recordDepthCopyCommands(drawCmdBuffers[i]);
		recordHizGenerationCommands(drawCmdBuffers[i]);

		VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
	}
}

void PBRTexture::recordRenderPassCommands(VkCommandBuffer cmdBuffer, const VkRenderPassBeginInfo& rpBeginInfo)
{
	auto descMgr = VulkanDescriptorManager::getManager();
	
	VkViewport viewport = vks::initializers::viewport(static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
	VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
	VkDeviceSize offsets[1] = {0};
	
	// shading
	vkCmdBeginRenderPass(cmdBuffer, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
	vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
	
	if (displaySkybox)
	{
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descMgr->getSet(DescriptorType::Scene, 4), 0, NULL);
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);
		vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RenderingPushConstants), &renderPushConstants);
		models.skybox.draw(cmdBuffer);
	}
	
	// shading
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadingPipeline.pipeline);
	vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadingPipeline.pipelineLayout, 0, 1, &descMgr->getSet(DescriptorType::shading, 0), 0, 0);
	vkCmdPushConstants(cmdBuffer, shadingPipeline.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RenderingPushConstants), &renderPushConstants);
	vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
	
	drawUI(cmdBuffer);
	vkCmdEndRenderPass(cmdBuffer);
	
	// final z 这里的布局回复
	VkImageSubresourceRange imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;	
	auto imgBarrier = createImageBarrier(finalZBuffer.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, imageSubresource);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imgBarrier);
	
	imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imgBarrier = createImageBarrier(finalVisBuffer.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, imageSubresource);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imgBarrier);
}


void PBRTexture::recordDepthCopyCommands(VkCommandBuffer cmdBuffer)
{
	auto descMgr = VulkanDescriptorManager::getManager();
	VkImageSubresourceRange depthRange = vks::vksTools::genDepthSubresourceRange();

	auto imgBarrier = createImageBarrier(depthStencil.image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, depthRange);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgBarrier);

	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, depthCopyPipeline.pipeline);
	vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, depthCopyPipeline.pipelineLayout, 0, 1, &descMgr->getSet(DescriptorType::depthCopy, 0), 0, nullptr);
	vkCmdDispatch(cmdBuffer, (width + WORKGROUP_SIZE_X - 1) / WORKGROUP_SIZE_X, (height + WORKGROUP_SIZE_Y - 1) / WORKGROUP_SIZE_Y, 1);

	imgBarrier = createImageBarrier(depthStencil.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, depthRange);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgBarrier);
	
	depthRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imgBarrier = createImageBarrier(textures.hizBuffer.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, depthRange);
	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imgBarrier);
}

void PBRTexture::recordHizGenerationCommands(VkCommandBuffer cmdBuffer)
{
	auto descMgr = VulkanDescriptorManager::getManager();
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, hizComputePipeline.pipeline);

	for (uint32_t mip = 0; mip < textures.hizBuffer.mipLevels - 1; ++mip)
	{
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, hizComputePipeline.pipelineLayout, 0, 1, &descMgr->getSet(DescriptorType::hiz, mip), 0, nullptr);
		vkCmdDispatch(cmdBuffer, (width + WORKGROUP_SIZE_X - 1) / WORKGROUP_SIZE_X, (height + WORKGROUP_SIZE_Y - 1) / WORKGROUP_SIZE_Y, 1);

		VkImageSubresourceRange mipRange{VK_IMAGE_ASPECT_COLOR_BIT, mip + 1, 1, 0, 1};
		auto imgBarrier = createImageBarrier(textures.hizBuffer.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, mipRange);
		vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgBarrier);
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
	
	uboCullingMatrices.currView = camera.matrices.view;
	uboCullingMatrices.currProj = camera.matrices.perspective;
	memcpy(cullingUniformBuffer.mapped, &uboCullingMatrices, sizeof(vks::UBOCullingMatrices));

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
		bool bNeedRebuildCommandBuffer = false;
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
			bNeedRebuildCommandBuffer = true;
		}
		if (overlay->checkBox("Software Rasterization", &cullingPushConstants.useSoftwareRasterization)) {
			bNeedRebuildCommandBuffer = true;
		}
		if (overlay->checkBox("Frustrum&Occlusion Culling", &cullingPushConstants.useFrustrumOcclusionCulling)) {
			bNeedRebuildCommandBuffer = true;
		}
		if (overlay->sliderInt("Threshold", &thresholdInt, 0, 1000))
		{
			bNeedRebuildCommandBuffer = true;
		}
		if (overlay->sliderInt("Visualize Clusters", &renderPushConstants.visClusters,0,3)) {
			bNeedRebuildCommandBuffer = true;
		}
		if (overlay->sliderInt("LOD level", &visClustersLevel, 0, naniteMesh.meshes.size() - 1))
		{
			bNeedRebuildCommandBuffer = true;
		}
		
		if (bNeedRebuildCommandBuffer)
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
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scene.visibleIndicesCount*sizeof(uint32_t), &hwRIndicesBuffer.buffer, &hwRIndicesBuffer.memory, nullptr));
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scene.visibleIndicesCount/3*sizeof(glm::uvec3), &hwRIDBuffer.buffer, &hwRIDBuffer.memory, nullptr));
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scene.visibleIndicesCount*sizeof(uint32_t), &swRIndicesBuffer.buffer, &swRIndicesBuffer.memory, nullptr));
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scene.visibleIndicesCount/3*sizeof(glm::uvec3), &swRIDBuffer.buffer, &swRIDBuffer.memory, nullptr));	
	
	for (auto& clusterInfo : scene.clusterInfo)
	{
		clusterInfos.emplace_back(clusterInfo);
	}

	vks::vksTools::createStagingBuffer(*this, 0, clusterInfos.size() * sizeof(Nanite::ClusterInfo), clusterInfos.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, clustersInfoBuffer);

	// 剔除用的uniform buffer
	uboCullingMatrices.model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	uboCullingMatrices.lastView = camera.matrices.view;
	uboCullingMatrices.lastProj = camera.matrices.perspective;
	uboCullingMatrices.currView = camera.matrices.view;
	uboCullingMatrices.currProj = camera.matrices.perspective;

	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(uboCullingMatrices), &cullingUniformBuffer.buffer, &cullingUniformBuffer.memory, &uboCullingMatrices));
	cullingUniformBuffer.device = device;
	VK_CHECK_RESULT(cullingUniformBuffer.map());

	drawIndexedIndirect.firstIndex = 0;
	drawIndexedIndirect.firstInstance = 0;
	drawIndexedIndirect.indexCount = models.object.indexBuffer.size();
	drawIndexedIndirect.instanceCount = 2;
	drawIndexedIndirect.vertexOffset = 0;

	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
	sizeof(drawIndexedIndirect), &drawIndexedIndirectBuffer.buffer, &drawIndexedIndirectBuffer.memory,
	 &drawIndexedIndirect));
	 
	drawIndexedIndirectBuffer.device = device;
	VK_CHECK_RESULT(drawIndexedIndirectBuffer.map());
	
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, 
	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(SWRIndirectBuffer), &swIndirectDispatchBuffer.buffer, &swIndirectDispatchBuffer.memory,nullptr));
	
	uint32_t numVertices = 0;
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	sizeof(uint32_t),
	&swNumVerticesBuffer.buffer, &swNumVerticesBuffer.memory, &numVertices
	));
	
	swNumVerticesBuffer.device = device;
	VK_CHECK_RESULT(swNumVerticesBuffer.map());
}

void PBRTexture::createErrorProjectionBuffers()
{
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scene.errorInfo.size()*sizeof(glm::vec2), &projectedErrorBuffer.buffer, &projectedErrorBuffer.memory, nullptr))

	for (auto& errorInfo : scene.errorInfo)
	{
		errorInfos.emplace_back(errorInfo);
	}
	vks::vksTools::createStagingBuffer(*this, 0, errorInfos.size() * sizeof(Nanite::ErrorInfo), errorInfos.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, errorInfoBuffer);

	// uniform init
	uboErrorMatrices.view = camera.matrices.view;
	uboErrorMatrices.proj = camera.matrices.perspective;
	uboErrorMatrices.camRight = camera.getRight();
	uboErrorMatrices.camUp = camera.getUp();
	VK_CHECK_RESULT(vulkanDevice->createBuffer( VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(vks::UBOErrorMatrices), &errorUniformBuffer.buffer, &errorUniformBuffer.memory, &uboErrorMatrices));

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

	for (int i = 0; i <= 3; i++)
	{
		for (int j = 0; j <= 3; j++)
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


// 图像资源描述
struct ImageResourceDesc
{
    VkFormat          format;
    VkImageUsageFlags usage;
    VkImageAspectFlags aspectMask;
    VkImageLayout     initialLayout;
    VkImageLayout     targetLayout;
    bool              createSampler;
};

// 创建单个图像资源 (Image + View + Memory + 可选Sampler)
void createImageResource(
    vks::VulkanDevice* vulkanDevice,
    VkDevice device,
    uint32_t width,
    uint32_t height,
    const ImageResourceDesc& desc,
    vks::RasterizeBuffer& outResource)  // 假设 ImageResource 包含 image, view, mem, sampler
{
    // 1. 创建 Image
    VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
    imageCI.imageType     = VK_IMAGE_TYPE_2D;
    imageCI.format        = desc.format;
    imageCI.extent        = { width, height, 1 };
    imageCI.mipLevels     = 1;
    imageCI.arrayLayers   = 1;
    imageCI.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage         = desc.usage;
    imageCI.initialLayout = desc.initialLayout;

    VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &outResource.image));

    // 2. 分配并绑定内存
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, outResource.image, &memReqs);

    VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
    memAlloc.allocationSize  = memReqs.size;
    memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(
        memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &outResource.memory));
    VK_CHECK_RESULT(vkBindImageMemory(device, outResource.image, outResource.memory, 0));

    // 3. 创建 ImageView
    VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
    viewCI.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewCI.image                           = outResource.image;
    viewCI.format                          = desc.format;
    viewCI.subresourceRange.aspectMask     = desc.aspectMask;
    viewCI.subresourceRange.baseMipLevel   = 0;
    viewCI.subresourceRange.levelCount     = 1;
    viewCI.subresourceRange.baseArrayLayer = 0;
    viewCI.subresourceRange.layerCount     = 1;

    VK_CHECK_RESULT(vkCreateImageView(device, &viewCI, nullptr, &outResource.view));

    // 4. 可选: 创建 Sampler
    if (desc.createSampler)
    {
        VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
        samplerCI.magFilter     = VK_FILTER_NEAREST;
        samplerCI.minFilter     = VK_FILTER_NEAREST;
        samplerCI.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.borderColor   = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

        VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &outResource.sampler));
    }
    else
    {
        outResource.sampler = VK_NULL_HANDLE;
    }
}

// 批量转换图像布局 (单个 command buffer)
void transitionImageLayouts(
    vks::VulkanDevice* vulkanDevice,
    VkQueue queue,
    const std::vector<std::pair<VkImage, VkImageAspectFlags>>& images,
    VkImageLayout oldLayout,
    VkImageLayout newLayout)
{
    VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    for (const auto& [image, aspectMask] : images)
    {
        VkImageSubresourceRange range = {};
        range.aspectMask = aspectMask;
        range.levelCount = 1;
        range.layerCount = 1;

        vks::tools::setImageLayout(cmdBuf, image, oldLayout, newLayout, range);
    }

    vulkanDevice->flushCommandBuffer(cmdBuf, queue);
}

// 优化后的 createRasterizeBuffer

void PBRTexture::createRasterizeBuffer()
{
    // 定义所有图像资源的配置
    struct BufferConfig
    {
        vks::RasterizeBuffer* resource;
        ImageResourceDesc desc;
    };

    std::vector<BufferConfig> bufferConfigs = {
        // HWRZBuffer: 硬件光栅化深度缓冲
        {
            &HWRasterizeBuffer,
            {
                .format        = VK_FORMAT_D32_SFLOAT,
                .usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .aspectMask    = VK_IMAGE_ASPECT_DEPTH_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .targetLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .createSampler = true
            }
        },
        // HWRVisBuffer: 硬件光栅化可见性缓冲
        {
            &HWRVisBuffer,
            {
                .format        = VK_FORMAT_R32_UINT,
                .usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .aspectMask    = VK_IMAGE_ASPECT_COLOR_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .targetLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .createSampler = false
            }
        },
        // FinalVisBuffer: 最终可见性缓冲 (Compute Shader 读写)
        {
            &finalVisBuffer,
            {
                .format        = VK_FORMAT_R32_UINT,
                .usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .aspectMask    = VK_IMAGE_ASPECT_COLOR_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .targetLayout  = VK_IMAGE_LAYOUT_GENERAL,
                .createSampler = false
            }
        },
        // FinalZBuffer: 最终深度缓冲 (Compute Shader 读写)
        {
            &finalZBuffer,
            {
                .format        = VK_FORMAT_R32_SFLOAT,
                .usage         = VK_IMAGE_USAGE_STORAGE_BIT,
                .aspectMask    = VK_IMAGE_ASPECT_COLOR_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .targetLayout  = VK_IMAGE_LAYOUT_GENERAL,
                .createSampler = false
            }
        },
        // SWRBuffer: 软件光栅化缓冲 (64-bit atomic)
        {
            &SWRasterizeBuffer,
            {
                .format        = VK_FORMAT_R64_UINT,
                .usage         = VK_IMAGE_USAGE_STORAGE_BIT,
                .aspectMask    = VK_IMAGE_ASPECT_COLOR_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .targetLayout  = VK_IMAGE_LAYOUT_GENERAL,
                .createSampler = false
            }
        }
    };
	
    for (const auto& config : bufferConfigs)
    {
        createImageResource(vulkanDevice, device, width, height, config.desc, *config.resource);
    }

    // 批量转换图像布局 (分组处理不同的目标布局)
    
    // Depth attachment layout
    {
        std::vector<std::pair<VkImage, VkImageAspectFlags>> depthImages = {
            { HWRasterizeBuffer.image, VK_IMAGE_ASPECT_DEPTH_BIT }
        };
        transitionImageLayouts(vulkanDevice, queue, depthImages,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    // Color attachment layout
    {
        std::vector<std::pair<VkImage, VkImageAspectFlags>> colorAttachmentImages = {
            { HWRVisBuffer.image, VK_IMAGE_ASPECT_COLOR_BIT }
        };
        transitionImageLayouts(vulkanDevice, queue, colorAttachmentImages,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    // General layout (for compute shader storage images)
    {
        std::vector<std::pair<VkImage, VkImageAspectFlags>> storageImages = {
            { finalVisBuffer.image, VK_IMAGE_ASPECT_COLOR_BIT },
            { finalZBuffer.image,   VK_IMAGE_ASPECT_COLOR_BIT },
            { SWRasterizeBuffer.image,      VK_IMAGE_ASPECT_COLOR_BIT }
        };
        transitionImageLayouts(vulkanDevice, queue, storageImages,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }

    // 创建 Uniform Buffer
    uboShading.invView = glm::inverse(camera.matrices.view);
    uboShading.invProj = glm::inverse(camera.matrices.perspective);
    uboShading.camPos  = camera.position * -1.0f;

    VK_CHECK_RESULT(vulkanDevice->createBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        sizeof(vks::UBOShading),
        &uniformBuffers.shadingMats.buffer,
        &uniformBuffers.shadingMats.memory,
        &uboShading));

    uniformBuffers.shadingMats.device = device;
    VK_CHECK_RESULT(uniformBuffers.shadingMats.map());
}

void PBRTexture::createModelMatsBuffer()
{
	vks::vksTools::createStagingBuffer(*this, 0, modelMats.size()*sizeof(glm::mat4), modelMats.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT, modelMatsBuffer, true);
}

void PBRTexture::createHWRasterizeFrameBuffer()
{
	VkImageView attachments[2];
	
	attachments[0] = HWRVisBuffer.view;
	attachments[1] = HWRasterizeBuffer.view;
	
	VkFramebufferCreateInfo framebufferCreateInfo = {};
	framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferCreateInfo.pNext = nullptr;
	framebufferCreateInfo.renderPass = hwRasterizeRenderPass;
	framebufferCreateInfo.attachmentCount = 2;
	framebufferCreateInfo.pAttachments = attachments;
	framebufferCreateInfo.width = width;
	framebufferCreateInfo.height = height;
	framebufferCreateInfo.layers = 1;
	
	VK_CHECK_RESULT(vkCreateFramebuffer(vulkanDevice->logicalDevice, &framebufferCreateInfo, nullptr, &HWRasterizeFrameBuffer));
}
