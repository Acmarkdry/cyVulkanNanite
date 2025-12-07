#include "Pipeline.h"

namespace cyRenderGraph
{
	// DepthSettings implementations
	DepthSettings DepthSettings::DepthTest()
	{
		DepthSettings settings;
		settings.depthFunc = vk::CompareOp::eLess;
		settings.writeEnable = true;
		return settings;
	}

	DepthSettings DepthSettings::Disabled()
	{
		DepthSettings settings;
		settings.depthFunc = vk::CompareOp::eAlways;
		settings.writeEnable = false;
		return settings;
	}

	DepthSettings DepthSettings::Always()
	{
		DepthSettings settings;
		settings.depthFunc = vk::CompareOp::eAlways;
		settings.writeEnable = true;
		return settings;
	}

	bool DepthSettings::operator < (const DepthSettings &other) const
	{
		return std::tie(depthFunc, writeEnable) < std::tie(other.depthFunc, other.writeEnable);
	}

	// BlendSettings implementations
	BlendSettings BlendSettings::Opaque()
	{
		BlendSettings blendSettings;
		blendSettings.blendState = vk::PipelineColorBlendAttachmentState()
			.setColorWriteMask(vk::ColorComponentFlagBits::eA | vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB)
			.setBlendEnable(false);
		return blendSettings;
	}

	BlendSettings BlendSettings::Add()
	{
		BlendSettings blendSettings;
		blendSettings.blendState = vk::PipelineColorBlendAttachmentState()
			.setBlendEnable(true)
			.setAlphaBlendOp(vk::BlendOp::eAdd)
			.setColorBlendOp(vk::BlendOp::eAdd)
			.setColorWriteMask(vk::ColorComponentFlagBits::eA | vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB)
			.setSrcColorBlendFactor(vk::BlendFactor::eOne)
			.setDstColorBlendFactor(vk::BlendFactor::eOne);
		return blendSettings;
	}

	BlendSettings BlendSettings::Mixed()
	{
		BlendSettings blendSettings;
		blendSettings.blendState = vk::PipelineColorBlendAttachmentState()
			.setBlendEnable(true)
			.setAlphaBlendOp(vk::BlendOp::eAdd)
			.setColorBlendOp(vk::BlendOp::eAdd)
			.setColorWriteMask(vk::ColorComponentFlagBits::eA | vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB)
			.setSrcColorBlendFactor(vk::BlendFactor::eOne)
			.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
			.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
			.setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha);
		return blendSettings;
	}

	BlendSettings BlendSettings::AlphaBlend()
	{
		BlendSettings blendSettings;
		blendSettings.blendState = vk::PipelineColorBlendAttachmentState()
			.setBlendEnable(true)
			.setAlphaBlendOp(vk::BlendOp::eAdd)
			.setColorBlendOp(vk::BlendOp::eAdd)
			.setColorWriteMask(vk::ColorComponentFlagBits::eA | vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB)
			.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
			.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
			.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
			.setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha);
		return blendSettings;
	}

	bool BlendSettings::operator < (const BlendSettings &other) const
	{
		return 
			std::tie(      blendState.blendEnable,       blendState.alphaBlendOp,       blendState.colorBlendOp,       blendState.srcColorBlendFactor,       blendState.dstColorBlendFactor) <
			std::tie(other.blendState.blendEnable, other.blendState.alphaBlendOp, other.blendState.colorBlendOp, other.blendState.srcColorBlendFactor, other.blendState.dstColorBlendFactor);
	}

	// GraphicsPipeline implementations
	vk::Pipeline GraphicsPipeline::GetHandle()
	{
		return pipeline.get();
	}

	vk::PipelineLayout GraphicsPipeline::GetLayout()
	{
		return pipelineLayout;
	}

	GraphicsPipeline::GraphicsPipeline(
		vk::Device logicalDevice,
		vk::ShaderModule vertexShader, vk::ShaderModule fragmentShader,
		const VertexDeclaration &vertexDecl,
		vk::PipelineLayout pipelineLayout,
		DepthSettings depthSettings,
		const std::vector<BlendSettings> &attachmentBlendSettings,
		vk::PrimitiveTopology primitiveTopology,
		vk::RenderPass renderPass)
	{
		this->pipelineLayout = pipelineLayout;
		auto vertexStageCreateInfo = vk::PipelineShaderStageCreateInfo()
			.setStage(vk::ShaderStageFlagBits::eVertex)
			.setModule(vertexShader)
			.setPName("main");

		auto fragmentStageCreateInfo = vk::PipelineShaderStageCreateInfo()
			.setStage(vk::ShaderStageFlagBits::eFragment)
			.setModule(fragmentShader)
			.setPName("main");

		vk::PipelineShaderStageCreateInfo shaderStageInfos[] = { vertexStageCreateInfo, fragmentStageCreateInfo };

		auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo()
			.setVertexBindingDescriptionCount(uint32_t(vertexDecl.GetBindingDescriptors().size()))
			.setPVertexBindingDescriptions(vertexDecl.GetBindingDescriptors().data())
			.setVertexAttributeDescriptionCount(uint32_t(vertexDecl.GetVertexAttributes().size()))
			.setPVertexAttributeDescriptions(vertexDecl.GetVertexAttributes().data());
		
		// 固定配置，基本没人动的
		auto inputAssemblyInfo = vk::PipelineInputAssemblyStateCreateInfo()
			.setTopology(primitiveTopology)
			.setPrimitiveRestartEnable(false);

		auto rasterizationStateInfo = vk::PipelineRasterizationStateCreateInfo()
			.setDepthClampEnable(false)
			.setPolygonMode(vk::PolygonMode::eFill) // 填充，wireless
			.setLineWidth(1.0f)
			.setCullMode(vk::CullModeFlagBits::eNone)
			.setFrontFace(vk::FrontFace::eClockwise) // 顺时针为正面
			.setDepthBiasEnable(false);

		auto multisampleStateInfo = vk::PipelineMultisampleStateCreateInfo()
			.setSampleShadingEnable(false)
			.setRasterizationSamples(vk::SampleCountFlagBits::e1); // no msaa

		std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachmentStates;
		for (const auto &blendSettings : attachmentBlendSettings)
		{
			colorBlendAttachmentStates.push_back(blendSettings.blendState);
		}

		auto colorBlendStateInfo = vk::PipelineColorBlendStateCreateInfo()
			.setLogicOpEnable(false)
			.setAttachmentCount(uint32_t(colorBlendAttachmentStates.size()))
			.setPAttachments(colorBlendAttachmentStates.data());

		auto depthStencilState = vk::PipelineDepthStencilStateCreateInfo()
			.setStencilTestEnable(false)
			// 如果 depthfunc == always 并且不写入 -> 完全关闭深度测试
			// 否则必须开启 因为vulkan不开depth test，就不能进行depth write 
			.setDepthTestEnable((depthSettings.depthFunc == vk::CompareOp::eAlways && !depthSettings.writeEnable) ? false : true)
			.setDepthCompareOp(depthSettings.depthFunc)
			.setDepthWriteEnable(depthSettings.writeEnable)
			.setDepthBoundsTestEnable(false);

		vk::DynamicState dynamicStates[] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		auto dynamicStateInfo = vk::PipelineDynamicStateCreateInfo()
			.setDynamicStateCount(2)
			.setPDynamicStates(dynamicStates);

		auto viewportState = vk::PipelineViewportStateCreateInfo()
			.setScissorCount(1)
			.setViewportCount(1);
		auto pipelineCreateInfo = vk::GraphicsPipelineCreateInfo()
			.setStageCount(2)
			.setPStages(shaderStageInfos)
			.setPVertexInputState(&vertexInputInfo)
			.setPInputAssemblyState(&inputAssemblyInfo)
			.setPRasterizationState(&rasterizationStateInfo)
			.setPViewportState(&viewportState)
			.setPMultisampleState(&multisampleStateInfo)
			.setPDepthStencilState(&depthStencilState)
			.setPColorBlendState(&colorBlendStateInfo)
			.setPDynamicState(&dynamicStateInfo)
			.setLayout(pipelineLayout)
			.setRenderPass(renderPass)
			.setSubpass(0)
			.setBasePipelineHandle(nullptr) //use later
			.setBasePipelineIndex(-1);

		pipeline = logicalDevice.createGraphicsPipelineUnique(nullptr, pipelineCreateInfo).value;
	}

	// ComputePipeline implementations
	vk::Pipeline ComputePipeline::GetHandle()
	{
		return pipeline.get();
	}

	vk::PipelineLayout ComputePipeline::GetLayout()
	{
		return pipelineLayout;
	}

	ComputePipeline::ComputePipeline(
		vk::Device logicalDevice,
		vk::ShaderModule computeShader,
		vk::PipelineLayout pipelineLayout)
	{
		this->pipelineLayout = pipelineLayout;
		auto computeStageCreateInfo = vk::PipelineShaderStageCreateInfo()
			.setStage(vk::ShaderStageFlagBits::eCompute)
			.setModule(computeShader)
			.setPName("main");

		auto viewportState = vk::PipelineViewportStateCreateInfo()
			.setScissorCount(1)
			.setViewportCount(1);
		auto pipelineCreateInfo = vk::ComputePipelineCreateInfo()
			.setFlags(vk::PipelineCreateFlags())
			.setStage(computeStageCreateInfo)
			.setLayout(pipelineLayout)
			.setBasePipelineHandle(nullptr) //use later
			.setBasePipelineIndex(-1);

		pipeline = logicalDevice.createComputePipelineUnique(nullptr, pipelineCreateInfo).value;
	}
}
