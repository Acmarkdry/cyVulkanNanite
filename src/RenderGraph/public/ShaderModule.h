#pragma once

#include <vulkan/vulkan.hpp>

namespace cyRenderGraph
{
	class Core;
	class ShaderModule
	{
	public:
		vk::ShaderModule GetHandle();
		uint32_t GetHash();
		ShaderModule(vk::Device device, const std::vector<uint32_t> &bytecode);
		
	private:
		void Init(vk::Device device, const std::vector<uint32_t> &bytecode);
		vk::UniqueShaderModule shaderModule;
		uint32_t hash;
		friend class Core;
	};
	
};
