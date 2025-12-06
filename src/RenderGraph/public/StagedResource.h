#pragma once
#include <memory>
#include <vulkan/vulkan.hpp>

#include "Buffer.h"

namespace cyRenderGraph
{
	class Buffer;

	class StagedBuffer
	{
	public:
		StagedBuffer(vk::PhysicalDevice physicalDevice, vk::Device logicalDevice, vk::DeviceSize size, vk::BufferUsageFlags bufferUsage)
		{
			this->size = size;
			stagingBuffer = std::unique_ptr<Buffer>(new Buffer(physicalDevice, logicalDevice, size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
			deviceLocalBuffer = std::unique_ptr<Buffer>(new Buffer(physicalDevice, logicalDevice, size, bufferUsage | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal));
		}
		void *Map()
		{
			return stagingBuffer->Map();
		}
		void Unmap(vk::CommandBuffer commandBuffer)
		{
			stagingBuffer->Unmap();
      
			auto copyRegion = vk::BufferCopy()
			  .setSrcOffset(0)
			  .setDstOffset(0)
			  .setSize(size);
			commandBuffer.copyBuffer(stagingBuffer->GetHandle(), deviceLocalBuffer->GetHandle(), { copyRegion });
		}
		vk::Buffer GetBuffer()
		{
			return deviceLocalBuffer->GetHandle();
		}
	
	private:
		std::unique_ptr<Buffer> stagingBuffer;
		std::unique_ptr<Buffer> deviceLocalBuffer;
		vk::DeviceSize size;
	};
	
}
