#pragma once
#include <vulkan/vulkan.hpp>

namespace cyRenderGraph
{
	class Sampler
	{
	public:
		vk::Sampler GetHandle() const;
		bool operator <(const Sampler& other) const;
		Sampler(vk::Device logicalDevice, vk::SamplerAddressMode addressMode, vk::Filter minMagFilterType, vk::SamplerMipmapMode mipFilterType, bool useComparison = false, vk::BorderColor borderColor = vk::BorderColor());

	private:
		vk::UniqueSampler samplerHandle;
	};
}
