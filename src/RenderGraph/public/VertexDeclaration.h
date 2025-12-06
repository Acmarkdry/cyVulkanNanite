#pragma once
#include<vulkan/vulkan.hpp>

namespace vk
{
	static bool operator <(const VertexInputBindingDescription& desc0, const VertexInputBindingDescription& desc1)
	{
		return std::tie(desc0.binding, desc0.inputRate, desc0.stride) < std::tie(desc1.binding, desc1.inputRate, desc1.stride);
	}

	static bool operator <(const VertexInputAttributeDescription& desc0, const VertexInputAttributeDescription& desc1)
	{
		return std::tie(desc0.binding, desc0.format, desc0.location, desc0.offset) < std::tie(desc1.binding, desc1.format, desc1.location, desc1.offset);
	}
}

namespace cyRenderGraph
{
	enum struct AttribTypes
	{
		floatType,
		vec2,
		vec3,
		vec4,
		u8vec3,
		u8vec4,
		i8vec4,
		color32
	};
	
	class VertexDeclaration
	{
	public:
		// 整个顶点buffer
		void AddVertexInputBinding(uint32_t bufferBinding, uint32_t stride)
		{
			auto bindingDesc = vk::VertexInputBindingDescription()
			                   .setBinding(bufferBinding)
			                   .setStride(stride)
			                   .setInputRate(vk::VertexInputRate::eVertex);
			bindingDescriptors.push_back(bindingDesc);
		}

		// buffer中的某个字段
		void AddVertexAttribute(uint32_t bufferBinding, uint32_t offset, AttribTypes attribType, uint32_t shaderLocation)
		{
			auto vertexAttribute = vk::VertexInputAttributeDescription()
			                       .setBinding(bufferBinding)
			                       .setFormat(ConvertAttribTypeToFormat(attribType))
			                       .setLocation(shaderLocation)
			                       .setOffset(offset);
			vertexAttributes.push_back(vertexAttribute);
		}

		const std::vector<vk::VertexInputBindingDescription>& GetBindingDescriptors() const
		{
			return bindingDescriptors;
		}

		const std::vector<vk::VertexInputAttributeDescription>& GetVertexAttributes() const
		{
			return vertexAttributes;
		}

		bool operator<(const VertexDeclaration& other) const
		{
			return std::tie(bindingDescriptors, vertexAttributes) < std::tie(other.bindingDescriptors, other.vertexAttributes);
		}

	private:
		static vk::Format ConvertAttribTypeToFormat(AttribTypes attribType)
		{
			switch (attribType)
			{
			case AttribTypes::floatType: return vk::Format::eR32Sfloat;
				break;
			case AttribTypes::vec2: return vk::Format::eR32G32Sfloat;
				break;
			case AttribTypes::vec3: return vk::Format::eR32G32B32Sfloat;
				break;
			case AttribTypes::vec4: return vk::Format::eR32G32B32A32Sfloat;
				break;
			case AttribTypes::u8vec3: return vk::Format::eR8G8B8Unorm;
			//用来存储0~255的整数
				break;
			case AttribTypes::u8vec4: return vk::Format::eR8G8B8A8Unorm;
			// TODO 这里有一个问题是数据都被/255了，不知道他们后续是怎么处理的
				break;
			case AttribTypes::i8vec4: return vk::Format::eR8G8B8A8Snorm;
				break;
			case AttribTypes::color32: return vk::Format::eR8G8B8A8Unorm;
				break;
			}
			return vk::Format::eUndefined;
		}

		std::vector<vk::VertexInputBindingDescription> bindingDescriptors;
		std::vector<vk::VertexInputAttributeDescription> vertexAttributes;
	};
}
