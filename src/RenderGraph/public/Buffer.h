#pragma once
#include <vulkan/vulkan.hpp>

namespace cyRenderGraph
{
	class Buffer
	{
	public:
		vk::Buffer GetHandle();
		vk::DeviceMemory GetMemory();
		void *Map();
		void Unmap();
		
		Buffer(vk::PhysicalDevice physicalDevice, vk::Device logicalDevice, vk::DeviceSize size, vk::BufferUsageFlags usageFlags, vk::MemoryPropertyFlags memoryVisibility);
		
	
		
	private:
		vk::UniqueBuffer bufferHandle;
		vk::UniqueDeviceMemory bufferMemory;
		vk::Device logicalDevice;
		vk::DeviceSize size;
	
	};
	
}

