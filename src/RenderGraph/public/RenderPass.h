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
		RenderPass(vk::Device logicalDevice, std::vector<AttachmentDesc> _colorAttachments, AttachmentDesc _depthAttachment);

	private:
		vk::UniqueRenderPass renderPass;
		std::vector<AttachmentDesc> colorAttachmentDescs;
		AttachmentDesc depthAttachmentDesc;
	};
}
