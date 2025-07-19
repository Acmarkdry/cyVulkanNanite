#include "vksTools.h"

#include "vulkanexamplebase.h"

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
}

