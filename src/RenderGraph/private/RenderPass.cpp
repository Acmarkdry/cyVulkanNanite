#include "RenderPass.h"

cyRenderGraph::RenderPass::RenderPass(vk::Device logicalDevice, std::vector<AttachmentDesc> _colorAttachments, AttachmentDesc _depthAttachment)
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

