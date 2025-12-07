#include "RenderGraphUtils.h"

namespace cyRenderGraph
{
	uint32_t FindMemoryTypeIndex(vk::PhysicalDevice physicalDevice, uint32_t suitableIndices, vk::MemoryPropertyFlags memoryVisiblity)
	{
		vk::PhysicalDeviceMemoryProperties availableMemoryProperties = physicalDevice.getMemoryProperties();
		
		for (uint32_t i = 0; i < availableMemoryProperties.memoryTypeCount; i++)
		{
			if ( (suitableIndices &(1<<i)) && ((availableMemoryProperties.memoryTypes[i].propertyFlags & memoryVisiblity) == memoryVisiblity)) 
			{
				return i;
			}
		}
		return uint32_t(-1);
	}
}


namespace cyRenderGraph
{
	
	
}

