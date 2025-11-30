#pragma once
#include <vulkan/vulkan.hpp>

namespace cyRenderGraph
{

	static uint32_t FindMemoryTypeIndex(vk::PhysicalDevice physicalDevice, uint32_t suitableIndices, vk::MemoryPropertyFlags memoryVisiblity);
	
	
}
