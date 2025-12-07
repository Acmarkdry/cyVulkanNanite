#include "ShaderModule.h"

vk::ShaderModule cyRenderGraph::ShaderModule::GetHandle()
{
	return shaderModule.get();
}

uint32_t cyRenderGraph::ShaderModule::GetHash()
{
	return hash;
}

cyRenderGraph::ShaderModule::ShaderModule(vk::Device device, const std::vector<uint32_t> &bytecode)
{
	Init(device, bytecode);
}

void cyRenderGraph::ShaderModule::Init(vk::Device device, const std::vector<uint32_t> &bytecode)
{
	auto shaderModuleCreateInfo = vk::ShaderModuleCreateInfo()
	  .setCodeSize(bytecode.size() * sizeof(uint32_t))
	  .setPCode(bytecode.data());
	this->shaderModule = device.createShaderModuleUnique(shaderModuleCreateInfo);
	this->hash = 0;
	for(auto b : bytecode)
	{
		this->hash ^= b; //actually terrible hash
	}
}

