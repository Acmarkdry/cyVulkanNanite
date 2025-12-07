#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include "RenderGraphUtils.h"

namespace cyRenderGraph
{
	bool IsDepthFormat(vk::Format format);
	vk::ImageUsageFlags GetGeneralUsageFlags(vk::Format format);

	static const vk::ImageUsageFlags colorImageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
	static const vk::ImageUsageFlags depthImageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;

	class Swapchain;
	class RenderTarget;
	class Image;

	struct ImageSubresourceRange
	{
		bool Contains(const ImageSubresourceRange& other);
		bool operator <(const ImageSubresourceRange& other) const;

		uint32_t baseMipLevel;
		uint32_t mipsCount;
		uint32_t baseArrayLayer;
		uint32_t arrayLayersCount;
	};

	class ImageData
	{
	public:
		vk::Image GetHandle() const;
		vk::Format GetFormat() const;
		vk::ImageType GetType() const;
		glm::uvec3 GetMipSize(uint32_t mipLevel);
		vk::ImageAspectFlags GetAspectFlags() const;
		uint32_t GetArrayLayersCount();
		uint32_t GetMipsCount();
		bool operator <(const ImageData& other) const;

	private:
		ImageData(vk::Image imageHandle, vk::ImageType imageType, glm::uvec3 size, uint32_t mipsCount, uint32_t arrayLayersCount, vk::Format format, vk::ImageLayout layout);
		void SetDebugName(std::string _debugName);

		struct SubImageInfo
		{
			vk::ImageLayout currLayout;
		};

		struct MipInfo
		{
			std::vector<SubImageInfo> layerInfos;
			glm::uvec3 size;
		};

		std::vector<MipInfo> mipInfos;

		vk::ImageAspectFlags aspectFlags;
		vk::Image imageHandle;
		vk::Format format;
		vk::ImageType imageType;
		uint32_t mipsCount;
		uint32_t arrayLayersCount;
		std::string debugName;

		friend class cyRenderGraph::Image;
		friend class cyRenderGraph::Swapchain;
		friend class Core;
	};

	class Image
	{
	public:
		cyRenderGraph::ImageData* GetImageData();
		vk::DeviceMemory GetMemory();
		
		static vk::ImageCreateInfo CreateInfo1d(glm::uint size, uint32_t mipsCount, uint32_t arrayLayersCount, vk::Format format, vk::ImageUsageFlags usage);
		static vk::ImageCreateInfo CreateInfo2d(glm::uvec2 size, uint32_t mipsCount, uint32_t arrayLayersCount, vk::Format format, vk::ImageUsageFlags usage);
		static vk::ImageCreateInfo CreateInfoVolume(glm::uvec3 size, uint32_t mipsCount, uint32_t arrayLayersCount, vk::Format format, vk::ImageUsageFlags usage);
		static vk::ImageCreateInfo CreateInfoCube(glm::uvec2 size, uint32_t mipsCount, vk::Format format, vk::ImageUsageFlags usage);

		Image(vk::PhysicalDevice physicalDevice, vk::Device logicalDevice, vk::ImageCreateInfo imageInfo, vk::MemoryPropertyFlags memFlags = vk::MemoryPropertyFlagBits::eDeviceLocal);

	private:
		vk::UniqueImage imageHandle;
		std::unique_ptr<ImageData> imageData;
		vk::UniqueDeviceMemory imageMemory;
		double bytesPerPixelAvg;
	};
}
