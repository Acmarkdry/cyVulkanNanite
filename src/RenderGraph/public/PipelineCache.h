#pragma once
#include "DescriptorSetCache.h"
#include "Pipeline.h"
#include "VertexDeclaration.h"
#include "vulkan/vulkan.hpp"

namespace cyRenderGraph
{
	class PipelineCache
	{
	public:
		PipelineCache(vk::Device _logicalDevice, DescriptorSetCache* _descriptorSetCache);

		struct PipelineInfo
		{
			vk::PipelineLayout pipelineLayout;
			std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;
		};

		PipelineInfo BindGraphicsPipeline(
			vk::CommandBuffer commandBuffer,
			vk::RenderPass renderPass,
			DepthSettings depthSettings,
			const std::vector<BlendSettings>& attachmentBlendSettings,
			VertexDeclaration vertexDeclaration,
			vk::PrimitiveTopology topology,
			ShaderProgram* shaderProgram);

		PipelineInfo BindComputePipeline(
			vk::CommandBuffer commandBuffer,
			Shader* computeShader);

		void Clear();

	private:
		struct PipelineLayoutKey
		{
			std::vector<vk::DescriptorSetLayout> setLayouts;
			bool operator <(const PipelineLayoutKey& other) const;
		};

		vk::UniquePipelineLayout CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& setLayouts);
		vk::PipelineLayout GetPipelineLayout(const PipelineLayoutKey& key);

		struct GraphicsPipelineKey
		{
			GraphicsPipelineKey();

			vk::ShaderModule vertexShaderModule;
			uint32_t vertexShaderHash;
			vk::ShaderModule fragmentShaderModule;
			uint32_t fragmentShaderHash;
			VertexDeclaration vertexDecl;
			vk::PipelineLayout pipelineLayout;
			vk::Extent2D extent;
			vk::RenderPass renderPass;
			DepthSettings depthSettings;
			std::vector<BlendSettings> attachmentBlendSettings;
			vk::PrimitiveTopology topology;
			bool operator <(const GraphicsPipelineKey& other) const;
		};

		GraphicsPipeline* GetGraphicsPipeline(const GraphicsPipelineKey& key);

		struct ComputePipelineKey
		{
			ComputePipelineKey();

			vk::ShaderModule computeShader;
			vk::PipelineLayout pipelineLayout;
			bool operator <(const ComputePipelineKey& other) const;
		};

		ComputePipeline* GetComputePipeline(const ComputePipelineKey& key);

		std::map<GraphicsPipelineKey, std::unique_ptr<GraphicsPipeline>> graphicsPipelineCache;
		std::map<ComputePipelineKey, std::unique_ptr<ComputePipeline>> computePipelineCache;
		std::map<PipelineLayoutKey, vk::UniquePipelineLayout> pipelineLayoutCache;
		DescriptorSetCache* descriptorSetCache;

		vk::Device logicalDevice;
	};
}
