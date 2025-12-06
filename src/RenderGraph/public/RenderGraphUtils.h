#pragma once
#include <vulkan/vulkan.hpp>


namespace cyRenderGraph
{

	static uint32_t FindMemoryTypeIndex(vk::PhysicalDevice physicalDevice, uint32_t suitableIndices, vk::MemoryPropertyFlags memoryVisiblity);
	
	// TODO 我没有看懂，我们是可以通过设置一个通用的头文件，然后这里面的都不写头文件，来做到加速吗？
	struct QueueFamilyIndices
	{
		uint32_t graphicsFamilyIndex;
		uint32_t presentFamilyIndex;
	};
		
}
