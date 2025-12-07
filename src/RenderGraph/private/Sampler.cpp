#include "Sampler.h"

vk::Sampler cyRenderGraph::Sampler::GetHandle() const
{
	return samplerHandle.get();
}

bool cyRenderGraph::Sampler::operator <(const Sampler& other) const
{
	return std::tie(samplerHandle.get()) < std::tie(other.samplerHandle.get());
}

cyRenderGraph::Sampler::Sampler(vk::Device logicalDevice, vk::SamplerAddressMode addressMode, vk::Filter minMagFilterType, vk::SamplerMipmapMode mipFilterType, bool useComparison, vk::BorderColor borderColor)
{
	auto samplerCreateInfo = vk::SamplerCreateInfo()
	                         .setAddressModeU(addressMode)
	                         .setAddressModeV(addressMode)
	                         .setAddressModeW(addressMode)
	                         .setAnisotropyEnable(false)
	                         .setCompareEnable(useComparison)
	                         .setCompareOp(useComparison ? vk::CompareOp::eLessOrEqual : vk::CompareOp::eAlways)
	                         .setMagFilter(minMagFilterType)
	                         .setMinFilter(minMagFilterType)
	                         .setMaxLod(1e7f)
	                         .setMinLod(0.0f)
	                         .setMipmapMode(mipFilterType)
	                         .setUnnormalizedCoordinates(false)
	                         .setBorderColor(borderColor);

	samplerHandle = logicalDevice.createSamplerUnique(samplerCreateInfo);
}

