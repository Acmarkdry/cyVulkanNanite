#pragma once
#include <vulkan/vulkan.hpp>

#include "ImageView.h"

namespace cyRenderGraph
{
	class Framebuffer
	{
	public:
		vk::Framebuffer GetHandle();
		Framebuffer(vk::Device logicalDevice, const std::vector<ImageView*> &imageViews, vk::Extent2D size, vk::RenderPass renderPass);
	private:
		vk::UniqueFramebuffer framebuffer;
	};
	
}

