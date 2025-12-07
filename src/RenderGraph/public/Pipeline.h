#pragma once
#include "VertexDeclaration.h"
#include "vulkan/vulkan.hpp"

namespace cyRenderGraph
{
	class Core;

	struct DepthSettings
	{
		static DepthSettings DepthTest();
		static DepthSettings Disabled();
		static DepthSettings Always();

		vk::CompareOp depthFunc;
		bool writeEnable;
		bool operator <(const DepthSettings& other) const;
	};

	struct BlendSettings
	{
		static BlendSettings Opaque();
		static BlendSettings Add();
		static BlendSettings Mixed();
		static BlendSettings AlphaBlend();

		bool operator <(const BlendSettings& other) const;
		vk::PipelineColorBlendAttachmentState blendState;
	};

	class GraphicsPipeline
	{
	public:
		enum struct BlendModes
		{
			Opaque
		};

		enum struct DepthStencilModes
		{
			DepthNone,
			DepthLess
		};

		vk::Pipeline GetHandle();
		vk::PipelineLayout GetLayout();

		GraphicsPipeline(
			vk::Device logicalDevice,
			vk::ShaderModule vertexShader, vk::ShaderModule fragmentShader,
			const VertexDeclaration& vertexDecl,
			vk::PipelineLayout pipelineLayout,
			DepthSettings depthSettings,
			const std::vector<BlendSettings>& attachmentBlendSettings,
			vk::PrimitiveTopology primitiveTopology,
			vk::RenderPass renderPass);

	private:
		vk::PipelineLayout pipelineLayout;
		vk::UniquePipeline pipeline;
		friend class Core;
	};

	class ComputePipeline
	{
	public:
		vk::Pipeline GetHandle();
		vk::PipelineLayout GetLayout();

		ComputePipeline(
			vk::Device logicalDevice,
			vk::ShaderModule computeShader,
			vk::PipelineLayout pipelineLayout);

	private:
		vk::PipelineLayout pipelineLayout;
		vk::UniquePipeline pipeline;
		friend class Core;
	};
}
