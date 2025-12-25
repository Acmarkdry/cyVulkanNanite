#include "Pipeline.h"

void Pipeline::destroy(VkDevice& device)
{
	vkDestroyPipeline(device, pipeline, nullptr);
}
