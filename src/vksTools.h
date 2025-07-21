#pragma once
#include "NaniteMesh/NaniteMesh.h"


class VulkanExampleBase;

namespace vks 
{
	class vksTools
	{
		public:

		void createStagingBuffer(VulkanExampleBase& variableLink, VkBufferUsageFlags sorceMemoryProperty, VkDeviceSize srcBufferSize,  void* srcBufferData, 
				VkBufferUsageFlags targetMemoryProperty, Buffer &targetStaingBuffer);
		
	};	
}

