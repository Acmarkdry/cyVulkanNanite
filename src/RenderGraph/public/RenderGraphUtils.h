#pragma once
#include <vulkan/vulkan.hpp>


namespace cyRenderGraph
{

	uint32_t FindMemoryTypeIndex(vk::PhysicalDevice physicalDevice, uint32_t suitableIndices, vk::MemoryPropertyFlags memoryVisiblity);
	
	struct QueueFamilyIndices
	{
		uint32_t graphicsFamilyIndex;
		uint32_t presentFamilyIndex;
	};
		
}
