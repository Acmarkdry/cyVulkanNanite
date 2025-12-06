#pragma once
#include "Buffer.h"
#include "ShaderProgram.h"
#include "vulkan/vulkan.hpp"

namespace cyRenderGraph
{
	class ShaderMemoryPool
	{
	public:
		ShaderMemoryPool(uint32_t _alignment)
		{
			alignment = _alignment;
		}

		void MapBuffer(Buffer* _buffer)
		{
			this->buffer = _buffer;
			dstMemory = buffer->Map();
			currOffset = 0;
			currSize = 0;
			currSetInfo = nullptr;
		}

		void UnmapBuffer()
		{
			buffer->Unmap();
			buffer = nullptr;
			dstMemory = nullptr;
		}

		Buffer* GetBuffer()
		{
			return buffer;
		}

		struct SetDynamicUniformBindings
		{
			std::vector<UniformBufferBinding> uniformBufferBindings;
			uint32_t dynamicOffset;
		};

		SetDynamicUniformBindings BeginSet(const DescriptorSetLayoutKey* setInfo)
		{
			this->currSetInfo = setInfo;
			currSize += currSetInfo->GetTotalConstantBufferSize();
			currSize = AlignSize(currSize, alignment);

			SetDynamicUniformBindings dynamicBindings;
			dynamicBindings.dynamicOffset = currOffset;

			std::vector<DescriptorSetLayoutKey::UniformBufferId> uniformBufferIds;
			uniformBufferIds.resize(setInfo->GetUniformBuffersCount());
			setInfo->GetUniformBufferIds(uniformBufferIds.data());

			uint32_t setUniformTotalSize = 0;
			for (auto uniformBufferId : uniformBufferIds)
			{
				auto uniformBufferInfo = setInfo->GetUniformBufferInfo(uniformBufferId);
				setUniformTotalSize += uniformBufferInfo.size;
				dynamicBindings.uniformBufferBindings.push_back(UniformBufferBinding(this->GetBuffer(), uniformBufferInfo.shaderBindingIndex, uniformBufferInfo.offsetInSet, uniformBufferInfo.size));
			}
			assert(currSetInfo->GetTotalConstantBufferSize() == setUniformTotalSize);

			return dynamicBindings;
		}

		void EndSet()
		{
			currOffset = currSize;
			currSetInfo = nullptr;
		}

		void* GetUniformBufferData(DescriptorSetLayoutKey::UniformBufferId uniformBufferId, size_t size)
		{
			auto bufferInfo = currSetInfo->GetUniformBufferInfo(uniformBufferId);
			assert(bufferInfo.size == size);
			size_t totalOffset = currOffset + bufferInfo.offsetInSet;
			assert(totalOffset + size <= currSize);
			return (static_cast<char*>(dstMemory) + totalOffset);
		}

		void* GetUniformBufferData(std::string bufferName, size_t size)
		{
			auto bufferId = currSetInfo->GetUniformBufferId(bufferName);
			assert(!(bufferId == DescriptorSetLayoutKey::UniformBufferId()));
			return GetUniformBufferData(bufferId, size);
		}

		template <typename BufferType>
		BufferType* GetUniformBufferData(DescriptorSetLayoutKey::UniformBufferId uniformBufferId)
		{
			auto bufferInfo = currSetInfo->GetUniformBufferInfo(uniformBufferId);
			size_t tmp = sizeof(BufferType);
			assert(bufferInfo.size == sizeof(BufferType));
			size_t totalOffset = currOffset + bufferInfo.offsetInSet;
			assert(totalOffset + sizeof(BufferType) <= currSize);
			return static_cast<BufferType*>((char*)dstMemory + totalOffset);
		}

		template <typename BufferType>
		BufferType* GetUniformBufferData(std::string bufferName)
		{
			auto bufferId = currSetInfo->GetUniformBufferId(bufferName);
			assert(!(bufferId == DescriptorSetLayoutKey::UniformBufferId()));
			return GetUniformBufferData<BufferType>(bufferId);
		}

		template <typename UniformType>
		UniformType* GetUniformData(std::string uniformName)
		{
			auto uniformId = currSetInfo->GetUniformId(uniformName);
			return GetUniformData<UniformType>(uniformId);
		}

		template <typename UniformType>
		UniformType* GetUniformData(DescriptorSetLayoutKey::UniformId uniformId)
		{
			auto uniformInfo = currSetInfo->GetUniformInfo(uniformId);
			auto bufferInfo = currSetInfo->GetUniformBufferInfo(uniformInfo.uniformBufferId);

			size_t totalOffset = currOffset + bufferInfo.offsetInSet + uniformInfo.offsetInBinding;
			assert(totalOffset + sizeof(UniformType) < currSize);
			return static_cast<UniformType*>((char*)dstMemory + totalOffset);
		}

	private:
		static uint32_t AlignSize(uint32_t size, uint32_t alignment)
		{
			uint32_t res_size = size;
			if (res_size % alignment != 0)
				res_size += (alignment - (res_size % alignment));
			return res_size;
		}

		uint32_t alignment;
		Buffer* buffer;
		const DescriptorSetLayoutKey* currSetInfo;
		void* dstMemory;
		uint32_t currOffset;
		uint32_t currSize;
	};
};
