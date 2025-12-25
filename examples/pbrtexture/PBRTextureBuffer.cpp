#include "PBRTextureBuffer.h"
#include <iostream>

void vks::Textures::destroy()
{
	environmentCube.destroy();
	irradianceCube.destroy();
	prefilteredCube.destroy();
	lutBrdf.destroy();
	albedoMap.destroy();
	normalMap.destroy();
	aoMap.destroy();
	metallicMap.destroy();
	roughnessMap.destroy();
	hizBuffer.destroy();
}

void vks::UniformBuffers::destroy()
{
	scene.destroy();
	skybox.destroy();
	params.destroy();
}

void vks::VulkanResourceTracker::createImageView(VkDevice& device, const VkImageViewCreateInfo& pCreateInfo, VkImageView& imageView)
{
	vkCreateImageView(device, &pCreateInfo, nullptr, &imageView);
	imageViewSet.insert(imageView);
}

void vks::VulkanResourceTracker::destroyImageView(VkDevice& device, VkImageView& view)
{
	vkDestroyImageView(device, view, nullptr);
	imageViewSet.erase(view);
		
}

void vks::VulkanResourceTracker::checkLeaks()
{
	if (!imageViewSet.empty())
	{
		for (auto &imageView : imageViewSet)
		{
			std::cout << "Image view not destroyed: " << imageView << std::endl;
		}
	}
}
