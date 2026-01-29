#include "Synchronization.h"

namespace cyRenderGraph
{
	ImageAccessPattern GetSrcImageAccessPattern(ImageUsageTypes usageType)
	{
		// 什么操作需要完成
		ImageAccessPattern accessPattern;
		switch (usageType)
		{
		//之前是只读的，最晚可能发生在fragment shader，read操作不需要处理access问题，eShaderReadOnlyOptimal是sampler的最优布局
		case ImageUsageTypes::GraphicsShaderRead:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eFragmentShader;
				accessPattern.accessMask = vk::AccessFlags();
				accessPattern.layout = vk::ImageLayout::eShaderReadOnlyOptimal;
				accessPattern.queueFamilyType = QueueFamilyTypes::Graphics;
			}
			break;
		// read/write，是采样器，可以在vertex或者fragment shader中写入
		// 有写操作，所以要处理access mask
		// 对于layout，general是最佳适配，唯一支持读写的布局
		case ImageUsageTypes::GraphicsShaderReadWrite:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader;
				accessPattern.accessMask = vk::AccessFlagBits::eShaderWrite;
				accessPattern.layout = vk::ImageLayout::eGeneral;
				accessPattern.queueFamilyType = QueueFamilyTypes::Graphics;
			}
			break;
		// 
		case ImageUsageTypes::ComputeShaderRead:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eComputeShader;
				accessPattern.accessMask = vk::AccessFlags();
				accessPattern.layout = vk::ImageLayout::eShaderReadOnlyOptimal;
				accessPattern.queueFamilyType = QueueFamilyTypes::Compute;
			}
			break;
		case ImageUsageTypes::ComputeShaderReadWrite:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eComputeShader;
				accessPattern.accessMask = vk::AccessFlagBits::eShaderWrite;
				accessPattern.layout = vk::ImageLayout::eGeneral;
				accessPattern.queueFamilyType = QueueFamilyTypes::Compute;
			}
			break;
		// 对于传输操作，一定有写操作
		// eTransferDstOptimal是最优传输布局
		case ImageUsageTypes::TransferDst:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eTransfer;
				accessPattern.accessMask = vk::AccessFlagBits::eTransferWrite;
				accessPattern.layout = vk::ImageLayout::eTransferDstOptimal;
				accessPattern.queueFamilyType = QueueFamilyTypes::Transfer;
			}
			break;
		// 传输起点，不需要flush
		case ImageUsageTypes::TransferSrc:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eTransfer;
				accessPattern.accessMask = vk::AccessFlags();
				accessPattern.layout = vk::ImageLayout::eTransferSrcOptimal;
				accessPattern.queueFamilyType = QueueFamilyTypes::Transfer;
			}
			break;
		// 写入color attachment的阶段
		// 因为是写入操作，所以一定有write
		// eColorAttachmentOptimal 最优解
		case ImageUsageTypes::ColorAttachment:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
				accessPattern.accessMask = vk::AccessFlagBits::eColorAttachmentWrite;
				accessPattern.layout = vk::ImageLayout::eColorAttachmentOptimal;
				accessPattern.queueFamilyType = QueueFamilyTypes::Graphics;
			}
			break;
		// 深度写入，发生在late fragment tests阶段，
		// 虽然early fragment test也可能写，但是late是最后保证完成的
		case ImageUsageTypes::DepthAttachment:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eLateFragmentTests;
				accessPattern.accessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
				accessPattern.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
				accessPattern.queueFamilyType = QueueFamilyTypes::Graphics;
			}
			break;
		// 最后的渲染操作 发生在管线的最后
		// 他只是讲图像发送到显示器，不写gpu内存
		case ImageUsageTypes::Present:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eBottomOfPipe;
				accessPattern.accessMask = vk::AccessFlags();
				accessPattern.layout = vk::ImageLayout::ePresentSrcKHR;
				accessPattern.queueFamilyType = QueueFamilyTypes::Present;
			}
			break;
		//
		case ImageUsageTypes::None:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eTopOfPipe;
				accessPattern.accessMask = vk::AccessFlags();
				accessPattern.layout = vk::ImageLayout::eUndefined;
				accessPattern.queueFamilyType = QueueFamilyTypes::Undefined;
			}
			break;
		//
		case ImageUsageTypes::Unknown:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eBottomOfPipe;
				accessPattern.accessMask = vk::AccessFlags();
				accessPattern.layout = vk::ImageLayout::eUndefined;
				accessPattern.queueFamilyType = QueueFamilyTypes::Undefined;
			}
			break;
		}
		return accessPattern;
	}

	ImageAccessPattern GetDstImageAccessPattern(ImageUsageTypes usageType)
	{
		// 什么操作需要等待
		ImageAccessPattern accessPattern;
		switch (usageType)
		{
		// vertex shader，图形管线最早可以采样纹理的阶段，在这里等待
		// read，强制让资源可见
		case ImageUsageTypes::GraphicsShaderRead:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eVertexShader;
				accessPattern.accessMask = vk::AccessFlagBits::eShaderRead;
				accessPattern.layout = vk::ImageLayout::eShaderReadOnlyOptimal;
				accessPattern.queueFamilyType = QueueFamilyTypes::Graphics;
			}
			break;
		// read write，就需要覆盖vertex shader和 fragement shader的范围
		// 同时确保写入操作可见
		case ImageUsageTypes::GraphicsShaderReadWrite:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader;
				accessPattern.accessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
				accessPattern.layout = vk::ImageLayout::eGeneral;
				accessPattern.queueFamilyType = QueueFamilyTypes::Graphics;
			}
			break;
		// 
		case ImageUsageTypes::ComputeShaderRead:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eComputeShader;
				accessPattern.accessMask = vk::AccessFlagBits::eShaderRead;
				accessPattern.layout = vk::ImageLayout::eShaderReadOnlyOptimal;
				accessPattern.queueFamilyType = QueueFamilyTypes::Compute;
			}
			break;
		// 
		case ImageUsageTypes::ComputeShaderReadWrite:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eComputeShader;
				accessPattern.accessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
				accessPattern.layout = vk::ImageLayout::eGeneral;
				accessPattern.queueFamilyType = QueueFamilyTypes::Compute;
			}
			break;
		// 作为copy/blit的目标，要求是一个写操作
		// 因为这里是要传输，所以要write
		case ImageUsageTypes::TransferDst:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eTransfer;
				accessPattern.accessMask = vk::AccessFlagBits::eTransferWrite;
				accessPattern.layout = vk::ImageLayout::eTransferDstOptimal;
				accessPattern.queueFamilyType = QueueFamilyTypes::Transfer;
			}
			break;
		// 作为源目标，要可以被read
		case ImageUsageTypes::TransferSrc:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eTransfer;
				accessPattern.accessMask = vk::AccessFlagBits::eTransferRead;
				accessPattern.layout = vk::ImageLayout::eTransferSrcOptimal;
				accessPattern.queueFamilyType = QueueFamilyTypes::Transfer;
			}
			break;
		// Read | Write 颜色附件可能涉及到write和read
		case ImageUsageTypes::ColorAttachment:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
				accessPattern.accessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
				accessPattern.layout = vk::ImageLayout::eColorAttachmentOptimal;
				accessPattern.queueFamilyType = QueueFamilyTypes::Graphics;
			}
			break;
		// depth要覆盖early和late两个fragment阶段
		case ImageUsageTypes::DepthAttachment:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eLateFragmentTests | vk::PipelineStageFlagBits::eEarlyFragmentTests;
				accessPattern.accessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
				accessPattern.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
				accessPattern.queueFamilyType = QueueFamilyTypes::Graphics;
			}
			break;
		// button of pipe，管线的最后
		case ImageUsageTypes::Present:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eBottomOfPipe;
				accessPattern.accessMask = vk::AccessFlags();
				accessPattern.layout = vk::ImageLayout::ePresentSrcKHR;
				accessPattern.queueFamilyType = QueueFamilyTypes::Present;
			}
			break;
		case ImageUsageTypes::None:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eBottomOfPipe;
				accessPattern.accessMask = vk::AccessFlags();
				accessPattern.layout = vk::ImageLayout::eUndefined;
				accessPattern.queueFamilyType = QueueFamilyTypes::Undefined;
			}
			break;
		case ImageUsageTypes::Unknown:
			{
				assert(0);
				accessPattern.stage = vk::PipelineStageFlagBits::eTopOfPipe;
				accessPattern.accessMask = vk::AccessFlags();
				accessPattern.layout = vk::ImageLayout::eUndefined;
				accessPattern.queueFamilyType = QueueFamilyTypes::Undefined;
			}
			break;
		}
		return accessPattern;
	}

	BufferAccessPattern GetSrcBufferAccessPattern(BufferUsageTypes usageType)
	{
		BufferAccessPattern accessPattern;
		switch (usageType)
		{
		case BufferUsageTypes::VertexBuffer:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eVertexInput;
				accessPattern.accessMask = vk::AccessFlags();
				accessPattern.queueFamilyType = QueueFamilyTypes::Graphics;
			}
			break;
		case BufferUsageTypes::GraphicsShaderReadWrite:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader;
				accessPattern.accessMask = vk::AccessFlagBits::eShaderWrite;
				accessPattern.queueFamilyType = QueueFamilyTypes::Graphics;
			}
			break;
		case BufferUsageTypes::ComputeShaderReadWrite:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eComputeShader;
				accessPattern.accessMask = vk::AccessFlagBits::eShaderWrite;
				accessPattern.queueFamilyType = QueueFamilyTypes::Compute;
			}
			break;
		case BufferUsageTypes::TransferDst:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eTransfer;
				accessPattern.accessMask = vk::AccessFlagBits::eTransferWrite;
				accessPattern.queueFamilyType = QueueFamilyTypes::Transfer;
			}
			break;
		case BufferUsageTypes::TransferSrc:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eTransfer;
				accessPattern.accessMask = vk::AccessFlags();
				accessPattern.queueFamilyType = QueueFamilyTypes::Transfer;
			}
			break;
		case BufferUsageTypes::None:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eTopOfPipe;
				accessPattern.accessMask = vk::AccessFlags();
				accessPattern.queueFamilyType = QueueFamilyTypes::Undefined;
			}
			break;
		case BufferUsageTypes::Unknown:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eBottomOfPipe;
				accessPattern.accessMask = vk::AccessFlags();
				accessPattern.queueFamilyType = QueueFamilyTypes::Undefined;
			}
			break;
		}
		return accessPattern;
	}

	BufferAccessPattern GetDstBufferAccessPattern(BufferUsageTypes usageType)
	{
		BufferAccessPattern accessPattern;
		switch (usageType)
		{
		case BufferUsageTypes::VertexBuffer:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eVertexInput;
				accessPattern.accessMask = vk::AccessFlagBits::eVertexAttributeRead;
				accessPattern.queueFamilyType = QueueFamilyTypes::Graphics;
			}
			break;
		case BufferUsageTypes::GraphicsShaderReadWrite:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader;
				accessPattern.accessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
				accessPattern.queueFamilyType = QueueFamilyTypes::Graphics;
			}
			break;
		case BufferUsageTypes::ComputeShaderReadWrite:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eComputeShader;
				accessPattern.accessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
				accessPattern.queueFamilyType = QueueFamilyTypes::Compute;
			}
			break;
		case BufferUsageTypes::TransferDst:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eTransfer;
				accessPattern.accessMask = vk::AccessFlagBits::eTransferWrite;
				accessPattern.queueFamilyType = QueueFamilyTypes::Transfer;
			}
			break;
		case BufferUsageTypes::TransferSrc:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eTransfer;
				accessPattern.accessMask = vk::AccessFlagBits::eTransferRead;
				accessPattern.queueFamilyType = QueueFamilyTypes::Transfer;
			}
			break;
		case BufferUsageTypes::None:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eBottomOfPipe;
				accessPattern.accessMask = vk::AccessFlags();
				accessPattern.queueFamilyType = QueueFamilyTypes::Undefined;
			}
			break;
		case BufferUsageTypes::Unknown:
			{
				accessPattern.stage = vk::PipelineStageFlagBits::eBottomOfPipe;
				accessPattern.accessMask = vk::AccessFlags();
				accessPattern.queueFamilyType = QueueFamilyTypes::Undefined;
			}
			break;
		}
		return accessPattern;
	}
}
