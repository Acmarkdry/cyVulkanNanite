#include "Buffer.h"
#include "RenderGraphUtils.h"

vk::Buffer cyRenderGraph::Buffer::GetHandle()
{
	return bufferHandle.get();
}

vk::DeviceMemory cyRenderGraph::Buffer::GetMemory()
{
	return bufferMemory.get();
}

void* cyRenderGraph::Buffer::Map()
{
	return logicalDevice.mapMemory(GetMemory(), 0, size);
}

void cyRenderGraph::Buffer::Unmap()
{
	logicalDevice.unmapMemory(GetMemory());
}

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
