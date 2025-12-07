#pragma once
#include <vulkan/vulkan.hpp>

#include "Image.h"

namespace vk
{
}

namespace cyRenderGraph
{
	class ImageView
	{
	public:
		vk::ImageView GetHandle() const;
		ImageData* GetImageData();
		const ImageData* GetImageData() const;
		uint32_t GetBaseMipLevel();
		uint32_t GetMipLevelsCount();
		uint32_t GetBaseArrayLayer();
		uint32_t GetArrayLayersCount();

		ImageView(vk::Device logicalDevice, ImageData* imageData, uint32_t baseMipLevel, uint32_t mipLevelsCount, uint32_t baseArrayLayer, uint32_t arrayLayersCount);
		ImageView(vk::Device logicalDevice, ImageData* cubemapImageData, uint32_t baseMipLevel, uint32_t mipLevelsCount);

	private:
		vk::UniqueImageView imageView;
		ImageData* imageData;
		uint32_t baseMipLevel;
		uint32_t mipLevelsCount;

		uint32_t baseArrayLayer;
		uint32_t arrayLayersCount;

		friend class Swapchain;
		friend class RenderTarget;
	};
}
