#pragma once
#include "NaniteMesh/NaniteMesh.h"


class PBRTexture;
class VulkanExampleBase;

namespace vks 
{
	class vksTools
	{
		public:

		void static createStagingBuffer(VulkanExampleBase& variableLink, VkBufferUsageFlags sorceMemoryProperty, VkDeviceSize srcBufferSize,  void* srcBufferData, 
				VkBufferUsageFlags targetMemoryProperty, Buffer &targetStaingBuffer);
		
		void static setPbrDescriptor(PBRTexture &pbrTexture);
		
		VkImageSubresourceRange static genDepthSubresourceRange();
	};	
}

