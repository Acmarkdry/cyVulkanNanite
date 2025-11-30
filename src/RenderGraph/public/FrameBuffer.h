#pragma once
#include <vulkan/vulkan.hpp>

namespace cyRenderGraph
{
	class ImageView;

	class FrameBuffer
	{
	public:
	vk::Framebuffer GetHandle();
	FrameBuffer(vk::Device logicalDevice, const std::vector<const ImageView*> &imageViews, vk::Extent2D size, vk::RenderPass renderPass);
	
	
	private:
		vk::UniqueFramebuffer frameBuffer;
	
	};
	
}

