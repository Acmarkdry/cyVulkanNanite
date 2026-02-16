#pragma once

#include <vulkan/vulkan.hpp>

/*
 * legitEngine实现的render graph中对于barrier有自己的理解
 * barriar的核心是为了防止race，所以对于dstStage去读取数据，要确保srcStage进行的写入已经彻底完成
 * 所以要解决两个问题：1.scr执行顺序优先于dst 2.scr的写操作，对于dst，是可见的
 * 所以我们可以进行对于render graph的追踪，如果上一步是read，就不需要处理
 * 如果上一步是write，则需要在scrAccessMask中，设置标志位
 */

namespace cyRenderGraph
{
	enum struct QueueFamilyTypes
	{
		Graphics,
		Transfer,
		Compute,
		Present,
		Undefined
	};

	enum struct ImageUsageTypes
	{
		GraphicsShaderRead,
		GraphicsShaderReadWrite,
		ComputeShaderRead,
		ComputeShaderReadWrite,
		TransferDst,
		TransferSrc,
		ColorAttachment,
		DepthAttachment,
		Present,
		None,
		Unknown
	};

	struct ImageAccessPattern
	{
		vk::PipelineStageFlags stage;
		vk::AccessFlags accessMask;
		vk::ImageLayout layout;
		QueueFamilyTypes queueFamilyType;
	};

	struct ImageSubresourceBarrier
	{
		ImageAccessPattern accessPattern;
		ImageAccessPattern dstAccessPattern;
	};

	ImageAccessPattern GetSrcImageAccessPattern(ImageUsageTypes usageType);

	ImageAccessPattern GetDstImageAccessPattern(ImageUsageTypes usageType);

	inline bool IsImageBarrierNeeded(ImageUsageTypes srcUsageType, ImageUsageTypes dstUsageType)
	{
		if (srcUsageType == ImageUsageTypes::GraphicsShaderRead && dstUsageType == ImageUsageTypes::GraphicsShaderRead) return false;
		return true;
	}

	struct BufferAccessPattern
	{
		vk::PipelineStageFlags stage;
		vk::AccessFlags accessMask;
		QueueFamilyTypes queueFamilyType;
	};

	struct BufferBarrier
	{
		ImageAccessPattern srcAccessPattern;
		ImageAccessPattern dstAccessPattern;
	};

	enum struct BufferUsageTypes
	{
		VertexBuffer,
		GraphicsShaderReadWrite,
		ComputeShaderReadWrite,
		TransferDst,
		TransferSrc,
		None,
		Unknown
	};

	BufferAccessPattern GetSrcBufferAccessPattern(BufferUsageTypes usageType);
	BufferAccessPattern GetDstBufferAccessPattern(BufferUsageTypes usageType);

	inline bool IsBufferBarrierNeeded(BufferUsageTypes scrUsageType, BufferUsageTypes dstUsageType)
	{
		return true;
	}
}
