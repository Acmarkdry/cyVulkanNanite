#pragma once
#include <vulkan/vulkan_core.h>

class Pipeline
{
public:
	VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
	VkPipeline pipeline{VK_NULL_HANDLE};

	void destroy(VkDevice& device);
};
