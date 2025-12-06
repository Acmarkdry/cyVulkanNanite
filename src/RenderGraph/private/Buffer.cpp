#include "Buffer.h"
#include "RenderGraphUtils.h"

cyRenderGraph::Buffer::Buffer(vk::PhysicalDevice physicalDevice, vk::Device logicalDevice, vk::DeviceSize size, vk::BufferUsageFlags usageFlags, vk::MemoryPropertyFlags memoryVisibility)
{
	this->logicalDevice = logicalDevice;
	this->size = size;
	auto bufferInfo = vk::BufferCreateInfo()
	  .setSize(size)
	  .setUsage(usageFlags)
	  .setSharingMode(vk::SharingMode::eExclusive);
	bufferHandle = logicalDevice.createBufferUnique(bufferInfo);

	vk::MemoryRequirements bufferMemRequirements = logicalDevice.getBufferMemoryRequirements(bufferHandle.get());

	auto allocInfo = vk::MemoryAllocateInfo()
	  .setAllocationSize(bufferMemRequirements.size)
	  .setMemoryTypeIndex(FindMemoryTypeIndex(physicalDevice, bufferMemRequirements.memoryTypeBits, memoryVisibility));

	bufferMemory = logicalDevice.allocateMemoryUnique(allocInfo);

	logicalDevice.bindBufferMemory(bufferHandle.get(), bufferMemory.get(), 0);
}
