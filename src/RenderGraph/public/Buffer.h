#pragma once
#include <vulkan/vulkan.hpp>

namespace cyRenderGraph
{
	class Buffer
	{
	public:
		vk::Buffer GetHandle()
		{
			return bufferHandle.get();
		}
		vk::DeviceMemory GetMemory()
		{
			return bufferMemory.get();
		}
		void *Map()
		{
			return logicalDevice.mapMemory(GetMemory(), 0, size);
		}
		void Unmap()
		{
			logicalDevice.unmapMemory(GetMemory());
		}
		
		Buffer(vk::PhysicalDevice physicalDevice, vk::Device logicalDevice, vk::DeviceSize size, vk::BufferUsageFlags usageFlags, vk::MemoryPropertyFlags memoryVisibility);
		
	
		
	private:
		vk::UniqueBuffer bufferHandle;
		vk::UniqueDeviceMemory bufferMemory;
		vk::Device logicalDevice;
		vk::DeviceSize size;
	
	};
	
}

