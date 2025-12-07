#include "VertexDeclaration.h"

namespace cyRenderGraph
{
	void VertexDeclaration::AddVertexInputBinding(uint32_t bufferBinding, uint32_t stride)
	{
		auto bindingDesc = vk::VertexInputBindingDescription()
		                   .setBinding(bufferBinding)
		                   .setStride(stride)
		                   .setInputRate(vk::VertexInputRate::eVertex);
		bindingDescriptors.push_back(bindingDesc);
	}

	void VertexDeclaration::AddVertexAttribute(uint32_t bufferBinding, uint32_t offset, AttribTypes attribType, uint32_t shaderLocation)
	{
		auto vertexAttribute = vk::VertexInputAttributeDescription()
		                       .setBinding(bufferBinding)
		                       .setFormat(ConvertAttribTypeToFormat(attribType))
		                       .setLocation(shaderLocation)
		                       .setOffset(offset);
		vertexAttributes.push_back(vertexAttribute);
	}

	const std::vector<vk::VertexInputBindingDescription>& VertexDeclaration::GetBindingDescriptors() const
	{
		return bindingDescriptors;
	}

	const std::vector<vk::VertexInputAttributeDescription>& VertexDeclaration::GetVertexAttributes() const
	{
		return vertexAttributes;
	}

	bool VertexDeclaration::operator<(const VertexDeclaration& other) const
	{
		return std::tie(bindingDescriptors, vertexAttributes) < std::tie(other.bindingDescriptors, other.vertexAttributes);
	}

	vk::Format VertexDeclaration::ConvertAttribTypeToFormat(AttribTypes attribType)
	{
		switch (attribType)
		{
		case AttribTypes::floatType: return vk::Format::eR32Sfloat;
		case AttribTypes::vec2: return vk::Format::eR32G32Sfloat;
		case AttribTypes::vec3: return vk::Format::eR32G32B32Sfloat;
		case AttribTypes::vec4: return vk::Format::eR32G32B32A32Sfloat;
		case AttribTypes::u8vec3: return vk::Format::eR8G8B8Unorm;
		case AttribTypes::u8vec4: return vk::Format::eR8G8B8A8Unorm;
		case AttribTypes::i8vec4: return vk::Format::eR8G8B8A8Snorm;
		case AttribTypes::color32: return vk::Format::eR8G8B8A8Unorm;
		}
		return vk::Format::eUndefined;
	}
}
