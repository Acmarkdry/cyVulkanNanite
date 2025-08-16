#pragma once
#include "NaniteMesh/NaniteMesh.h"


class PBRTexture;
class VulkanExampleBase;

namespace vks
{
	class vksTools
	{
	public:
		void static createStagingBuffer(VulkanExampleBase& variableLink, VkBufferUsageFlags sorceMemoryProperty, VkDeviceSize srcBufferSize, void* srcBufferData, VkBufferUsageFlags targetMemoryProperty, vkglTF::Model::Vertices& targetStaingBuffer, bool cmdRestart = true);

		void static createStagingBuffer(VulkanExampleBase& variableLink, VkBufferUsageFlags sorceMemoryProperty, VkDeviceSize srcBufferSize, void* srcBufferData, VkBufferUsageFlags targetMemoryProperty, vkglTF::Model::Indices& targetStaingBuffer, bool cmdRestart = true);

		void static createStagingBuffer(VulkanExampleBase& variableLink, VkBufferUsageFlags sorceMemoryProperty, VkDeviceSize srcBufferSize, void* srcBufferData, VkBufferUsageFlags targetMemoryProperty, Buffer& targetStaingBuffer, bool cmdRestart = true);

		void static setPbrDescriptor(PBRTexture& pbrTexture);

		VkImageSubresourceRange static genDepthSubresourceRange();

		void static generateBRDFLUT(PBRTexture& pbrTexture);
		void static generateIrradianceCube(PBRTexture& pbrTexture);
		void static generatePrefilteredCube(PBRTexture& pbrTexture);
	};
}
