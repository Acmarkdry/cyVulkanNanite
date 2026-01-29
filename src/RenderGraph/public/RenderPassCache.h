#pragma once
#include <algorithm>
#include <map>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <memory>

#include "FrameBuffer.h"
#include "RenderPass.h"

namespace cyRenderGraph
{
	class RenderPassCache
	{
	public:
		RenderPassCache(vk::Device _logicalDevice) : logicalDevice(_logicalDevice)
		{
		}

		struct RenderPassKey
		{
			RenderPassKey()
			{
			}

			std::vector<AttachmentDesc> colorAttachmentDescs;
			AttachmentDesc depthAttachmentDesc;

			bool operator <(const RenderPassKey& other) const
			{
				return std::tie(this->colorAttachmentDescs, this->depthAttachmentDesc) < std::tie(other.colorAttachmentDescs, other.depthAttachmentDesc);
			}
		};

		RenderPass* GetRenderPass(const RenderPassKey& key)
		{
			auto& renderPass = renderPassCache[key];
			if (!renderPass)
			{
				renderPass = std::unique_ptr<RenderPass>(new RenderPass(logicalDevice, key.colorAttachmentDescs, key.depthAttachmentDesc));
			}
			return renderPass.get();
		}

	private:
		std::map<RenderPassKey, std::unique_ptr<RenderPass>> renderPassCache;
		vk::Device logicalDevice;
	};
	
	// TODO vulkan的framebuffer感觉有点过度设计了，是可以被简化掉的 -> dynamic state
	// 初期的设计是为了兼容TBDR
	class FramebufferCache
	{
	public:
		struct PassInfo
		{
			Framebuffer* framebuffer;
			RenderPass* renderPass;
		};

		struct Attachment
		{
			ImageView* imageView;
			vk::ClearValue clearValue;
		};

		PassInfo BeginPass(vk::CommandBuffer commandBuffer, const std::vector<Attachment> colorAttachments, Attachment* depthAttachment, RenderPass* renderPass, vk::Extent2D renderAreaExtent)
		{
			PassInfo passInfo;
			
			FramebufferKey framebufferKey;

			std::vector<vk::ClearValue> clearValues;

			size_t attachmentsUsed = 0;
			for (auto attachment : colorAttachments)
			{
				clearValues.push_back(attachment.clearValue);
				framebufferKey.colorAttachmentViews[attachmentsUsed++] = attachment.imageView;
			}
			if (depthAttachment)
			{
				framebufferKey.depthAttachmentView = depthAttachment->imageView;
				clearValues.push_back(depthAttachment->clearValue);
			}

			passInfo.renderPass = renderPass;

			framebufferKey.extent = renderAreaExtent;
			framebufferKey.renderPass = renderPass->GetHandle();
			Framebuffer* framebuffer = GetFramebuffer(framebufferKey);
			passInfo.framebuffer = framebuffer;
			
			auto rect = vk::Rect2D(vk::Offset2D(), renderAreaExtent);
			auto passBeginInfo = vk::RenderPassBeginInfo()
								 .setRenderPass(renderPass->GetHandle())
								 .setFramebuffer(framebuffer->GetHandle())
								 .setRenderArea(rect)
								 .setClearValueCount(static_cast<uint32_t>(clearValues.size()))
								 .setPClearValues(clearValues.data());

			commandBuffer.beginRenderPass(passBeginInfo, vk::SubpassContents::eInline);

			auto viewport = vk::Viewport()
							.setWidth(static_cast<float>(renderAreaExtent.width))
							.setHeight(static_cast<float>(renderAreaExtent.height))
							.setMinDepth(0.0f)
							.setMaxDepth(1.0f);

			commandBuffer.setViewport(0, {viewport});
			commandBuffer.setScissor(0, {vk::Rect2D(vk::Offset2D(), renderAreaExtent)});

			return passInfo;
		}

		void EndPass(vk::CommandBuffer commandBuffer)
		{
			commandBuffer.endRenderPass();
		}

		FramebufferCache(vk::Device _logicalDevice) : logicalDevice(_logicalDevice)
		{
		}

	private:
		struct FramebufferKey
		{
			FramebufferKey()
			{
				std::fill(colorAttachmentViews.begin(), colorAttachmentViews.end(), nullptr);
				depthAttachmentView = nullptr;
				renderPass = nullptr;
			}

			std::array<const ImageView*, 8> colorAttachmentViews;
			const ImageView* depthAttachmentView;
			vk::Extent2D extent;
			vk::RenderPass renderPass;

			bool operator <(const FramebufferKey& other) const
			{
				return std::tie(colorAttachmentViews, depthAttachmentView, extent.width, extent.height) < std::tie(other.colorAttachmentViews, other.depthAttachmentView, other.extent.width, other.extent.height);
			}
		};
		
		
		Framebuffer* GetFramebuffer(FramebufferKey key)
		{
			auto& framebuffer = framebufferCache[key];

			if (!framebuffer)
			{
				std::vector<const ImageView*> imageViews;
				for (auto imageView : key.colorAttachmentViews)
				{
					if (imageView)
						imageViews.push_back(imageView);
				}
				if (key.depthAttachmentView)
					imageViews.push_back(key.depthAttachmentView);

				framebuffer = std::unique_ptr<Framebuffer>(new Framebuffer(logicalDevice, imageViews, key.extent, key.renderPass));
			}
			return framebuffer.get();
		}

		std::map<FramebufferKey, std::unique_ptr<Framebuffer>> framebufferCache;

		vk::Device logicalDevice;
	};
}