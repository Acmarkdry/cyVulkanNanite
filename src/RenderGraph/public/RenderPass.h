#pragma once
#include <vector>
#include <vulkan/vulkan.hpp>

namespace vk
{
	enum class AttachmentLoadOp;
	enum class Format;
	union ClearValue;
	
	static bool operator < (const vk::ClearValue &v0, const vk::ClearValue &v1)
	{
		return
		  std::tie(v0.color.int32[0], v0.color.int32[1], v0.color.int32[2], v0.color.int32[3]) <
		  std::tie(v1.color.int32[0], v1.color.int32[1], v1.color.int32[2], v1.color.int32[3]);
	}
}

namespace cyRenderGraph
{
	struct AttachmentDesc
	{
		vk::Format format;
		vk::AttachmentLoadOp loadOP;
		vk::ClearValue clearValue;

		bool operator <(const AttachmentDesc& other) const
		{
			return std::tie(format, loadOP, clearValue) < std::tie(other.format, other.loadOP, other.clearValue);
		}
	};

	class RenderPass
	{
	public:
		RenderPass(vk::Device logicalDevice, std::vector<AttachmentDesc> _colorAttachments, AttachmentDesc _depthAttachment)
		{
			this->colorAttachmentDescs = _colorAttachments;
			this->depthAttachmentDesc = _depthAttachment;

			std::vector<vk::AttachmentReference> colorAttachmentRefs;

			uint32_t currAttachmentIndex = 0;

			std::vector<vk::AttachmentDescription> attachmentDescs;
			for (auto colorAttachmentDesc : colorAttachmentDescs)
			{

				colorAttachmentRefs.push_back(vk::AttachmentReference()
				                              .setAttachment(currAttachmentIndex++)
				                              .setLayout(vk::ImageLayout::eColorAttachmentOptimal));

				auto attachmentDesc = vk::AttachmentDescription()
				                      .setFormat(colorAttachmentDesc.format)
				                      .setSamples(vk::SampleCountFlagBits::e1)
				                      .setLoadOp(colorAttachmentDesc.loadOP)
				                      .setStoreOp(vk::AttachmentStoreOp::eStore)
				                      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
				                      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
				                      .setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				                      .setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);
				attachmentDescs.push_back(attachmentDesc);
			}


			auto subpass = vk::SubpassDescription()
			               .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
			               .setColorAttachmentCount(static_cast<uint32_t>(colorAttachmentRefs.size()))
			               .setPColorAttachments(colorAttachmentRefs.data());

			vk::AttachmentReference depthRef;
			if (depthAttachmentDesc.format != vk::Format::eUndefined)
			{
				depthRef.setAttachment(currAttachmentIndex++)
				.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

				auto attachmentDesc = vk::AttachmentDescription()
				                      .setFormat(depthAttachmentDesc.format)
				                      .setSamples(vk::SampleCountFlagBits::e1)
				                      .setLoadOp(depthAttachmentDesc.loadOP)
				                      .setStoreOp(vk::AttachmentStoreOp::eStore)
				                      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
				                      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
				                      .setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
				                      .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
				attachmentDescs.push_back(attachmentDesc);

				subpass.setPDepthStencilAttachment(&depthRef);
			}

			auto renderPassInfo = vk::RenderPassCreateInfo()
			                      .setAttachmentCount(static_cast<uint32_t>(attachmentDescs.size()))
			                      .setPAttachments(attachmentDescs.data())
			                      .setSubpassCount(1)
			                      .setPSubpasses(&subpass)
			                      .setDependencyCount(0)
			                      .setPDependencies(nullptr);
			
			this->renderPass = logicalDevice.createRenderPassUnique(renderPassInfo);
		}

	private:
		vk::UniqueRenderPass renderPass;
		std::vector<AttachmentDesc> colorAttachmentDescs;
		AttachmentDesc depthAttachmentDesc;
	};
}
