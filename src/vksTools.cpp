#include "vksTools.h"

#include "VulkanDescriptorManager.h"
#include "vulkanexamplebase.h"
#include "../examples/pbrtexture/pbrTexture.h"

namespace vks
{
	void vksTools::createStagingBuffer(VulkanExampleBase& variableLink, VkBufferUsageFlags sorceMemoryProperty, VkDeviceSize srcBufferSize,  void* srcBufferData, 
		VkBufferUsageFlags targetMemoryProperty, Buffer &targetStaingBuffer)
	{
		VulkanDevice* vulkanDevice = variableLink.vulkanDevice;
		VkQueue& queue = variableLink.GetQueue();
		Buffer srcStagingBuffer;
		
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | sorceMemoryProperty,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			srcBufferSize,
			&srcStagingBuffer.buffer,
			&srcStagingBuffer.memory,
			srcBufferData))

		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT | targetMemoryProperty,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			srcBufferSize,
			&targetStaingBuffer.buffer,
			&targetStaingBuffer.memory,
			nullptr))

		VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		VkBufferCopy copyRegion = {};

		copyRegion.size = srcBufferSize;
		vkCmdCopyBuffer(copyCmd, srcStagingBuffer.buffer, srcStagingBuffer.buffer, 1, &copyRegion);

		vulkanDevice->flushCommandBuffer(copyCmd, queue, true);
		vkDestroyBuffer(vulkanDevice->logicalDevice, srcStagingBuffer.buffer, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, srcStagingBuffer.memory, nullptr);
	}

	void vksTools::setPbrDescriptor(PBRTexture& pbrTexture)
	{
		auto descMgr = VulkanDescriptorManager::getManager();
		// scene
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			                                              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT,
			                                              1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			                                              VK_SHADER_STAGE_FRAGMENT_BIT, 2),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			                                              VK_SHADER_STAGE_FRAGMENT_BIT, 3),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			                                              VK_SHADER_STAGE_FRAGMENT_BIT, 4),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			                                              VK_SHADER_STAGE_FRAGMENT_BIT, 5),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			                                              VK_SHADER_STAGE_FRAGMENT_BIT, 6),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			                                              VK_SHADER_STAGE_FRAGMENT_BIT, 7),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			                                              VK_SHADER_STAGE_FRAGMENT_BIT, 8),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			                                              VK_SHADER_STAGE_FRAGMENT_BIT, 9),
		};
		descMgr->addSetLayout(DescriptorType::Scene, setLayoutBindings, 6);
		
		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1),
		};
		descMgr->addSetLayout(DescriptorType::hiz, setLayoutBindings, pbrTexture.textures.hizBuffer.mipLevels - 1);
		
		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, 0)
		};
		descMgr->addSetLayout(DescriptorType::debugQuad, setLayoutBindings, 1);
		
		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1),
		};
		descMgr->addSetLayout(DescriptorType::depthCopy, setLayoutBindings, 1);


		descMgr->createLayoutsAndSets(pbrTexture.GetDevice());
		auto uniformBuffers = pbrTexture.uniformBuffers;
		auto textures = pbrTexture.textures;
		auto hizImageViews = pbrTexture.hizImageViews;
		
		descMgr->writeToSet(DescriptorType::Scene, 0, 0, &uniformBuffers.scene.descriptor);
		descMgr->writeToSet(DescriptorType::Scene, 0, 1, &uniformBuffers.params.descriptor);
		descMgr->writeToSet(DescriptorType::Scene, 0, 2, &textures.irradianceCube.descriptor);
		descMgr->writeToSet(DescriptorType::Scene, 0, 3, &textures.lutBrdf.descriptor);
		descMgr->writeToSet(DescriptorType::Scene, 0, 4, &textures.prefilteredCube.descriptor);
		descMgr->writeToSet(DescriptorType::Scene, 0, 5, &textures.albedoMap.descriptor);
		descMgr->writeToSet(DescriptorType::Scene, 0, 6, &textures.normalMap.descriptor);
		descMgr->writeToSet(DescriptorType::Scene, 0, 7, &textures.aoMap.descriptor);
		descMgr->writeToSet(DescriptorType::Scene, 0, 8, &textures.metallicMap.descriptor);
		descMgr->writeToSet(DescriptorType::Scene, 0, 9, &textures.roughnessMap.descriptor);

		descMgr->writeToSet(DescriptorType::Scene, 4, 0, &uniformBuffers.skybox.descriptor);
		descMgr->writeToSet(DescriptorType::Scene, 4, 1, &uniformBuffers.params.descriptor);
		descMgr->writeToSet(DescriptorType::Scene, 4, 2, &textures.environmentCube.descriptor);
		
		// hiz build
		for (int i = 0; i < textures.hizBuffer.mipLevels-1; i++)
		{
			VkDescriptorImageInfo inputImage = vks::initializers::descriptorImageInfo(nullptr, hizImageViews[i], VK_IMAGE_LAYOUT_GENERAL);
			VkDescriptorImageInfo outputImage = vks::initializers::descriptorImageInfo(nullptr, hizImageViews[i + 1], VK_IMAGE_LAYOUT_GENERAL);

			descMgr->writeToSet(DescriptorType::hiz, i, 0, &inputImage);
			descMgr->writeToSet(DescriptorType::hiz, i, 1, &outputImage);
		}
		
		// debug quad
		descMgr->writeToSet(DescriptorType::debugQuad, 0, 0, &textures.hizBuffer.descriptor);
		
		// depth copy
		VkDescriptorImageInfo depthStencilImage = vks::initializers::descriptorImageInfo(pbrTexture.depthStencilSampler, pbrTexture.depthStencil.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		VkDescriptorImageInfo outputImage = vks::initializers::descriptorImageInfo(nullptr, hizImageViews[0], VK_IMAGE_LAYOUT_GENERAL);
		
		descMgr->writeToSet(DescriptorType::depthCopy, 0, 0, &depthStencilImage);
		descMgr->writeToSet(DescriptorType::depthCopy, 0, 1, &outputImage);
	}

	VkImageSubresourceRange vksTools::genDepthSubresourceRange()
	{
		VkImageSubresourceRange subresourceRange{};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.layerCount = 1;

		return subresourceRange;
	}
}

