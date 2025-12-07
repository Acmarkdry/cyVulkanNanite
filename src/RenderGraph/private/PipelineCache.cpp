#include "PipelineCache.h"

namespace cyRenderGraph
{
	PipelineCache::PipelineCache(vk::Device _logicalDevice, DescriptorSetCache *_descriptorSetCache) :
		logicalDevice(_logicalDevice),
		descriptorSetCache(_descriptorSetCache)
	{
	}

	PipelineCache::PipelineInfo PipelineCache::BindGraphicsPipeline(
		vk::CommandBuffer commandBuffer,
		vk::RenderPass renderPass,
		DepthSettings depthSettings,
		const std::vector<BlendSettings> &attachmentBlendSettings,
		VertexDeclaration vertexDeclaration,
		vk::PrimitiveTopology topology,
		ShaderProgram *shaderProgram)
	{
		GraphicsPipelineKey pipelineKey;
		pipelineKey.vertexShaderModule = shaderProgram->vertexShader->GetModule()->GetHandle();
		pipelineKey.vertexShaderHash = shaderProgram->vertexShader->GetModule()->GetHash();
		pipelineKey.fragmentShaderModule = shaderProgram->fragmentShader->GetModule()->GetHandle();
		pipelineKey.fragmentShaderHash = shaderProgram->fragmentShader->GetModule()->GetHash();
		pipelineKey.vertexDecl = vertexDeclaration;
		pipelineKey.depthSettings = depthSettings;
		pipelineKey.attachmentBlendSettings = attachmentBlendSettings;
		pipelineKey.topology = topology;

		PipelineInfo pipelineInfo;

		Shader *targetShader = shaderProgram->vertexShader;

		PipelineLayoutKey pipelineLayoutKey;
		for (auto &setLayoutKey : shaderProgram->combinedDescriptorSetLayoutKeys)
		{
			pipelineLayoutKey.setLayouts.push_back(descriptorSetCache->GetDescriptorSetLayout(setLayoutKey));
		}

		pipelineKey.pipelineLayout = GetPipelineLayout(pipelineLayoutKey);

		pipelineKey.renderPass = renderPass;

		GraphicsPipeline *pipeline = GetGraphicsPipeline(pipelineKey);

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->GetHandle());

		pipelineInfo.pipelineLayout = pipeline->GetLayout();
		return pipelineInfo;
	}

	PipelineCache::PipelineInfo PipelineCache::BindComputePipeline(
		vk::CommandBuffer commandBuffer,
		Shader *computeShader)
	{
		ComputePipelineKey pipelineKey;
		pipelineKey.computeShader = computeShader->GetModule()->GetHandle();

		PipelineInfo pipelineInfo;
		Shader *targetShader = computeShader;

		PipelineLayoutKey pipelineLayoutKey;
		pipelineLayoutKey.setLayouts.resize(computeShader->GetSetsCount());
		for (size_t setIndex = 0; setIndex < pipelineLayoutKey.setLayouts.size(); setIndex++)
		{
			vk::DescriptorSetLayout setLayoutHandle = nullptr;
			auto computeSetInfo = computeShader->GetSetInfo(setIndex);
			if (!computeSetInfo->IsEmpty())
				setLayoutHandle = descriptorSetCache->GetDescriptorSetLayout(*computeSetInfo);

			pipelineLayoutKey.setLayouts[setIndex] = setLayoutHandle;
		}
		pipelineKey.pipelineLayout = GetPipelineLayout(pipelineLayoutKey);

		ComputePipeline *pipeline = GetComputePipeline(pipelineKey);

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline->GetHandle());

		pipelineInfo.pipelineLayout = pipeline->GetLayout();
		return pipelineInfo;
	}

	void PipelineCache::Clear()
	{
		this->computePipelineCache.clear();
		this->graphicsPipelineCache.clear();
		this->pipelineLayoutCache.clear();
	}

	bool PipelineCache::PipelineLayoutKey::operator < (const PipelineLayoutKey &other) const
	{
		return std::tie(setLayouts) < std::tie(other.setLayouts);
	}

	vk::UniquePipelineLayout PipelineCache::CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout> &setLayouts)
	{
		auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
			.setPushConstantRangeCount(0)
			.setPPushConstantRanges(nullptr)
			.setSetLayoutCount(uint32_t(setLayouts.size()))
			.setPSetLayouts(setLayouts.data());

		return logicalDevice.createPipelineLayoutUnique(pipelineLayoutInfo);
	}

	vk::PipelineLayout PipelineCache::GetPipelineLayout(const PipelineLayoutKey &key)
	{
		auto &pipelineLayout = pipelineLayoutCache[key];
		if (!pipelineLayout)
			pipelineLayout = CreatePipelineLayout(key.setLayouts);
		return pipelineLayout.get();
	}

	PipelineCache::GraphicsPipelineKey::GraphicsPipelineKey()
	{
		vertexShaderModule = nullptr;
		fragmentShaderModule = nullptr;
		renderPass = nullptr;
	}

	bool PipelineCache::GraphicsPipelineKey::operator < (const GraphicsPipelineKey &other) const
	{
		return
			std::tie(vertexShaderModule, vertexShaderHash, fragmentShaderModule, fragmentShaderHash, vertexDecl, pipelineLayout, renderPass, depthSettings, attachmentBlendSettings, topology) <
			std::tie(other.vertexShaderModule, other.vertexShaderHash, other.fragmentShaderModule, other.fragmentShaderHash, other.vertexDecl, other.pipelineLayout, other.renderPass, other.depthSettings, other.attachmentBlendSettings, topology);
	}

	GraphicsPipeline *PipelineCache::GetGraphicsPipeline(const GraphicsPipelineKey &key)
	{
		auto &pipeline = graphicsPipelineCache[key];
		if (!pipeline)
			pipeline = std::unique_ptr<GraphicsPipeline>(new GraphicsPipeline(logicalDevice, key.vertexShaderModule, key.fragmentShaderModule, key.vertexDecl, key.pipelineLayout, key.depthSettings, key.attachmentBlendSettings, key.topology, key.renderPass));
		return pipeline.get();
	}

	PipelineCache::ComputePipelineKey::ComputePipelineKey()
	{
		computeShader = nullptr;
	}

	bool PipelineCache::ComputePipelineKey::operator < (const ComputePipelineKey &other) const
	{
		return
			std::tie(computeShader, pipelineLayout) <
			std::tie(other.computeShader, other.pipelineLayout);
	}

	ComputePipeline *PipelineCache::GetComputePipeline(const ComputePipelineKey &key)
	{
		auto &pipeline = computePipelineCache[key];
		if (!pipeline)
			pipeline = std::unique_ptr<ComputePipeline>(new ComputePipeline(logicalDevice, key.computeShader, key.pipelineLayout));
		return pipeline.get();
	}
}
