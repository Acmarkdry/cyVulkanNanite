#include "RenderGraph.h"
#include "RenderPassCache.h"

namespace cyRenderGraph
{
	void RenderGraph::Execute(vk::CommandBuffer commandBuffer, CpuProfiler* cpuProfiler, GpuProfiler* gpuProfiler)
	{
		// 先拿到资源进行复用
		ResolveImages();
		ResolveImageViews();
		ResolveBuffers();

		for (size_t taskIndex = 0; taskIndex < tasks.size(); ++taskIndex)
		{
			auto& task = tasks[taskIndex];
			switch (task.type)
			{
			case Task::Types::RenderPass:
				{
					// 启动性能分析
					auto& renderPassDesc = renderPassDescs[task.index];
					auto profilerTask = CreateProfilerTask(renderPassDesc);
					auto gpuTask = gpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color, vk::PipelineStageFlagBits::eBottomOfPipe);
					auto cpuTask = cpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color);

					// 开始构建pass context，进行资源解析
					RenderPassContext passContext;
					passContext.resolvedImageViews.resize(imageViewProxies.GetSize(), nullptr);
					passContext.resolvedBuffers.resize(bufferProxies.GetSize(), nullptr);

					for (auto& inputImageViewProxy : renderPassDesc.inputImageViewProxies)
					{
						passContext.resolvedImageViews[inputImageViewProxy.asInt] = GetResolvedImageView(taskIndex, inputImageViewProxy);
					}

					for (auto& inoutStorageImageProxy : renderPassDesc.inoutStorageImageProxies)
					{
						passContext.resolvedImageViews[inoutStorageImageProxy.asInt] = GetResolvedImageView(taskIndex, inoutStorageImageProxy);
					}

					for (auto& inoutBufferProxy : renderPassDesc.inoutStorageBufferProxies)
					{
						passContext.resolvedBuffers[inoutBufferProxy.asInt] = GetResolvedBuffer(taskIndex, inoutBufferProxy);
					}

					for (auto& vertexBufferProxy : renderPassDesc.vertexBufferProxies)
					{
						passContext.resolvedBuffers[vertexBufferProxy.asInt] = GetResolvedBuffer(taskIndex, vertexBufferProxy);
					}

					vk::PipelineStageFlags srcStage;
					vk::PipelineStageFlags dstStage;
					std::vector<vk::ImageMemoryBarrier> imageBarriers;

					// 最关键的部分，开始插入核心同步逻辑
					for (auto inputImageViewProxy : renderPassDesc.inputImageViewProxies)
					{
						auto imageView = GetResolvedImageView(taskIndex, inputImageViewProxy);
						// 输入图像，转换到shader read布局
						AddImageTransitionBarriers(imageView, ImageUsageTypes::GraphicsShaderRead, taskIndex, srcStage, dstStage, imageBarriers);
					}

					for (auto& inoutStorageImageProxy : renderPassDesc.inoutStorageImageProxies)
					{
						auto imageView = GetResolvedImageView(taskIndex, inoutStorageImageProxy);
						// storage图像，转换到shader read write
						AddImageTransitionBarriers(imageView, ImageUsageTypes::GraphicsShaderReadWrite, taskIndex, srcStage, dstStage, imageBarriers);
					}

					for (auto colorAttachment : renderPassDesc.colorAttachments)
					{
						auto imageView = GetResolvedImageView(taskIndex, colorAttachment.imageViewProxyId);
						// color attachment
						AddImageTransitionBarriers(imageView, ImageUsageTypes::ColorAttachment, taskIndex, srcStage, dstStage, imageBarriers);
					}

					// 有点绕的写法，检测师范设置了
					// TODO 优化一下
					if (!(renderPassDesc.depthAttachment.imageViewProxyId == ImageViewProxyId()))
					{
						auto imageView = GetResolvedImageView(taskIndex, renderPassDesc.depthAttachment.imageViewProxyId);
						// depth attachment
						AddImageTransitionBarriers(imageView, ImageUsageTypes::DepthAttachment, taskIndex, srcStage, dstStage, imageBarriers);
					}

					std::vector<vk::BufferMemoryBarrier> bufferBarriers;

					for (auto vertexBufferProxy : renderPassDesc.vertexBufferProxies)
					{
						auto storageBuffer = GetResolvedBuffer(taskIndex, vertexBufferProxy);
						AddBufferBarriers(storageBuffer, BufferUsageTypes::VertexBuffer, taskIndex, srcStage, dstStage, bufferBarriers);
					}

					for (auto inoutBufferProxy : renderPassDesc.inoutStorageBufferProxies)
					{
						auto storageBuffer = GetResolvedBuffer(taskIndex, inoutBufferProxy);
						AddBufferBarriers(storageBuffer, BufferUsageTypes::GraphicsShaderReadWrite, taskIndex, srcStage, dstStage, bufferBarriers);
					}

					// 一次性提交barrier
					if (imageBarriers.size() > 0 || bufferBarriers.size() > 0) commandBuffer.pipelineBarrier(srcStage, dstStage, vk::DependencyFlags(), {}, bufferBarriers, imageBarriers);

					// 开始构建render pass meta data
					std::vector<FramebufferCache::Attachment> colorAttachments;
					FramebufferCache::Attachment depthAttachment;

					RenderPassCache::RenderPassKey renderPassKey;

					for (auto& attachment : renderPassDesc.colorAttachments)
					{
						auto imageView = GetResolvedImageView(taskIndex, attachment.imageViewProxyId);

						renderPassKey.colorAttachmentDescs.push_back({imageView->GetImageData()->GetFormat(), attachment.loadOp, attachment.clearValue});
						colorAttachments.push_back({imageView, attachment.clearValue});
					}
					bool depthPresent = !(renderPassDesc.depthAttachment.imageViewProxyId == ImageViewProxyId());
					if (depthPresent)
					{
						auto imageView = GetResolvedImageView(taskIndex, renderPassDesc.depthAttachment.imageViewProxyId);

						renderPassKey.depthAttachmentDesc = {imageView->GetImageData()->GetFormat(), renderPassDesc.depthAttachment.loadOp, renderPassDesc.depthAttachment.clearValue};
						depthAttachment = {imageView, renderPassDesc.depthAttachment.clearValue};
					}
					else
					{
						renderPassKey.depthAttachmentDesc.format = vk::Format::eUndefined;
					}

					auto renderPass = renderPassCache.GetRenderPass(renderPassKey);
					passContext.renderPass = renderPass;

					// record Command and end
					frameBufferCache.BeginPass(commandBuffer, colorAttachments, depthPresent ? (&depthAttachment) : nullptr, renderPass, renderPassDesc.renderAreaExtent);
					passContext.commandBuffer = commandBuffer;
					renderPassDesc.recordFunc(passContext);
					frameBufferCache.EndPass(commandBuffer);
				}
				break;
			case Task::Types::ComputePass:
				{
					// 和render pass很像，但是不需要framebuffer和render pass meta info
					auto& computePassDesc = computePassDescs[task.index];
					auto profilerTask = CreateProfilerTask(computePassDesc);
					auto gpuTask = gpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color, vk::PipelineStageFlagBits::eBottomOfPipe);
					auto cpuTask = cpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color);

					PassContext passContext;
					passContext.resolvedImageViews.resize(imageViewProxies.GetSize(), nullptr);
					passContext.resolvedBuffers.resize(bufferProxies.GetSize(), nullptr);

					for (auto& inputImageViewProxy : computePassDesc.inputImageViewProxies)
					{
						passContext.resolvedImageViews[inputImageViewProxy.asInt] = GetResolvedImageView(taskIndex, inputImageViewProxy);
					}

					for (auto& inoutBufferProxy : computePassDesc.inoutStorageBufferProxies)
					{
						passContext.resolvedBuffers[inoutBufferProxy.asInt] = GetResolvedBuffer(taskIndex, inoutBufferProxy);
					}

					for (auto& inoutStorageImageProxy : computePassDesc.inoutStorageImageProxies)
					{
						passContext.resolvedImageViews[inoutStorageImageProxy.asInt] = GetResolvedImageView(taskIndex, inoutStorageImageProxy);
					}

					vk::PipelineStageFlags srcStage;
					vk::PipelineStageFlags dstStage;

					// 对于render pass，不需要color/depth attachment和vertex buffer，因为compute shader不走rasterize
					// 注意这里用compute shader read/compute shader write
					std::vector<vk::ImageMemoryBarrier> imageBarriers;
					for (auto inputImageViewProxy : computePassDesc.inputImageViewProxies)
					{
						auto imageView = GetResolvedImageView(taskIndex, inputImageViewProxy);
						AddImageTransitionBarriers(imageView, ImageUsageTypes::ComputeShaderRead, taskIndex, srcStage, dstStage, imageBarriers);
					}

					for (auto& inoutStorageImageProxy : computePassDesc.inoutStorageImageProxies)
					{
						auto imageView = GetResolvedImageView(taskIndex, inoutStorageImageProxy);
						AddImageTransitionBarriers(imageView, ImageUsageTypes::ComputeShaderReadWrite, taskIndex, srcStage, dstStage, imageBarriers);
					}

					std::vector<vk::BufferMemoryBarrier> bufferBarriers;
					for (auto inoutBufferProxy : computePassDesc.inoutStorageBufferProxies)
					{
						auto storageBuffer = GetResolvedBuffer(taskIndex, inoutBufferProxy);
						AddBufferBarriers(storageBuffer, BufferUsageTypes::ComputeShaderReadWrite, taskIndex, srcStage, dstStage, bufferBarriers);
					}

					if (imageBarriers.size() > 0 || bufferBarriers.size() > 0) commandBuffer.pipelineBarrier(srcStage, dstStage, vk::DependencyFlags(), {}, bufferBarriers, imageBarriers);

					passContext.commandBuffer = commandBuffer;
					if (computePassDesc.recordFunc) computePassDesc.recordFunc(passContext);
				}
				break;
			case Task::Types::TransferPass:
				{
					// 为数据copy准备的
					auto& transferPassDesc = transferPassDescs[task.index];
					auto profilerTask = CreateProfilerTask(transferPassDesc);
					auto gpuTask = gpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color, vk::PipelineStageFlagBits::eBottomOfPipe);
					auto cpuTask = cpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color);

					PassContext passContext;
					passContext.resolvedImageViews.resize(imageViewProxies.GetSize(), nullptr);
					passContext.resolvedBuffers.resize(bufferProxies.GetSize(), nullptr);

					// 资源分src和dst
					for (auto& srcImageViewProxy : transferPassDesc.srcImageViewProxies)
					{
						passContext.resolvedImageViews[srcImageViewProxy.asInt] = GetResolvedImageView(taskIndex, srcImageViewProxy);
					}
					for (auto& dstImageViewProxy : transferPassDesc.dstImageViewProxies)
					{
						passContext.resolvedImageViews[dstImageViewProxy.asInt] = GetResolvedImageView(taskIndex, dstImageViewProxy);
					}

					for (auto& srcBufferProxy : transferPassDesc.srcBufferProxies)
					{
						passContext.resolvedBuffers[srcBufferProxy.asInt] = GetResolvedBuffer(taskIndex, srcBufferProxy);
					}

					for (auto& dstBufferProxy : transferPassDesc.dstBufferProxies)
					{
						passContext.resolvedBuffers[dstBufferProxy.asInt] = GetResolvedBuffer(taskIndex, dstBufferProxy);
					}

					vk::PipelineStageFlags srcStage;
					vk::PipelineStageFlags dstStage;

					std::vector<vk::ImageMemoryBarrier> imageBarriers;
					for (auto srcImageViewProxy : transferPassDesc.srcImageViewProxies)
					{
						auto imageView = GetResolvedImageView(taskIndex, srcImageViewProxy);
						AddImageTransitionBarriers(imageView, ImageUsageTypes::TransferSrc, taskIndex, srcStage, dstStage, imageBarriers);
					}

					for (auto dstImageViewProxy : transferPassDesc.dstImageViewProxies)
					{
						auto imageView = GetResolvedImageView(taskIndex, dstImageViewProxy);
						AddImageTransitionBarriers(imageView, ImageUsageTypes::TransferDst, taskIndex, srcStage, dstStage, imageBarriers);
					}

					std::vector<vk::BufferMemoryBarrier> bufferBarriers;
					for (auto srcBufferProxy : transferPassDesc.srcBufferProxies)
					{
						auto storageBuffer = GetResolvedBuffer(taskIndex, srcBufferProxy);
						AddBufferBarriers(storageBuffer, BufferUsageTypes::TransferSrc, taskIndex, srcStage, dstStage, bufferBarriers);
					}

					for (auto dstBufferProxy : transferPassDesc.dstBufferProxies)
					{
						auto storageBuffer = GetResolvedBuffer(taskIndex, dstBufferProxy);
						AddBufferBarriers(storageBuffer, BufferUsageTypes::TransferDst, taskIndex, srcStage, dstStage, bufferBarriers);
					}

					if (imageBarriers.size() > 0 || bufferBarriers.size() > 0) commandBuffer.pipelineBarrier(srcStage, dstStage, vk::DependencyFlags(), {}, bufferBarriers, imageBarriers);

					passContext.commandBuffer = commandBuffer;
					if (transferPassDesc.recordFunc) transferPassDesc.recordFunc(passContext);
				}
				break;
			case Task::Types::ImagePresent:
				{
					// 将swapchanin的layout转换为Present
					auto imagePesentDesc = imagePresentDescs[task.index];
					auto profilerTask = CreateProfilerTask(imagePesentDesc);
					auto gpuTask = gpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color, vk::PipelineStageFlagBits::eBottomOfPipe);
					auto cpuTask = cpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color);

					vk::PipelineStageFlags srcStage;
					vk::PipelineStageFlags dstStage;
					std::vector<vk::ImageMemoryBarrier> imageBarriers;
					{
						auto imageView = GetResolvedImageView(taskIndex, imagePesentDesc.presentImageViewProxyId);
						AddImageTransitionBarriers(imageView, ImageUsageTypes::Present, taskIndex, srcStage, dstStage, imageBarriers);
					}

					if (imageBarriers.size() > 0) commandBuffer.pipelineBarrier(srcStage, dstStage, vk::DependencyFlags(), {}, {}, imageBarriers);
				}
				break;
			case Task::Types::FrameSyncBegin:
				{
					// 插入一个帧同步 从buttom of pipe到top of pipe，
					auto frameSyncDesc = frameSyncBeginDescs[task.index];
					auto profilerTask = CreateProfilerTask(frameSyncDesc);
					auto gpuTask = gpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color, vk::PipelineStageFlagBits::eBottomOfPipe);
					auto cpuTask = cpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color);

					std::vector<vk::ImageMemoryBarrier> imageBarriers;
					vk::PipelineStageFlags srcStage = vk::PipelineStageFlagBits::eBottomOfPipe;
					vk::PipelineStageFlags dstStage = vk::PipelineStageFlagBits::eTopOfPipe;

					auto memoryBarrier = vk::MemoryBarrier();
					commandBuffer.pipelineBarrier(srcStage, dstStage, vk::DependencyFlags(), {memoryBarrier}, {}, {});
				}
				break;
			case Task::Types::FrameSyncEnd:
				{
					// 遍历所有外部image view，恢复到外部期望的状态
					auto frameSyncDesc = frameSyncEndDescs[task.index];
					auto profilerTask = CreateProfilerTask(frameSyncDesc);
					auto gpuTask = gpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color, vk::PipelineStageFlagBits::eBottomOfPipe);
					auto cpuTask = cpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color);

					std::vector<vk::ImageMemoryBarrier> imageBarriers;
					vk::PipelineStageFlags srcStart = vk::PipelineStageFlagBits::eBottomOfPipe;
					vk::PipelineStageFlags dstStart = vk::PipelineStageFlagBits::eTopOfPipe;

					for (auto imageViewProxy : imageViewProxies)
					{
						if (imageViewProxy.externalView != nullptr && imageViewProxy.externalUsageType != ImageUsageTypes::Unknown && imageViewProxy.externalUsageType != ImageUsageTypes::None)
						{
							AddImageTransitionBarriers(imageViewProxy.externalView, imageViewProxy.externalUsageType, taskIndex, srcStart, dstStart, imageBarriers);
						}
					}

					if (imageBarriers.size() > 0) commandBuffer.pipelineBarrier(srcStart, dstStart, vk::DependencyFlags(), {}, {}, imageBarriers);
				}
				break;
			}
		}

		FlushExternalImages(commandBuffer, cpuProfiler, gpuProfiler);

		renderPassDescs.clear();
		transferPassDescs.clear();
		imagePresentDescs.clear();
		frameSyncBeginDescs.clear();
		frameSyncEndDescs.clear();
		tasks.clear();
	}

	bool RenderGraph::ImageViewContainsSubresource(ImageView* imageView, ImageData* imageData, uint32_t mipLevel, uint32_t arrayLayer)
	{
		// 因为每一张图像的不同mip level可能在同一帧被不同pass以不同用途使用，所以barrier需要精确到sub resource级别才能正确
		return (imageView->GetImageData() == imageData && arrayLayer >= imageView->GetBaseArrayLayer() && arrayLayer < imageView->GetBaseArrayLayer() + imageView->GetArrayLayersCount() && mipLevel >= imageView->GetBaseMipLevel() && mipLevel < imageView->GetBaseMipLevel() + imageView->GetMipLevelsCount());
	}

	ImageUsageTypes RenderGraph::GetTaskImageSubresourceUsageType(size_t taskIndex, ImageData* imageData, uint32_t mipLevel, uint32_t arrayLayer)
	{
		// 一个有点高级和麻烦的玩意
		// 通过判断image miplevel arraylayer查出这个task把子资源当做什么用途使用
		// 主要用途是通过GetLastImageSubresourceUsageType，来查找某个子资源上一次被用作什么，从而知道barrier需要从什么布局转换到什么布局
		// 精确到mip+layer级别的查询保证barrier即使过于保守也不会被遗漏
		Task& task = tasks[taskIndex];
		switch (task.type)
		{
		case Task::Types::RenderPass:
			{
				// attachment优先于shader read，如果同一个子资源即使color attachment，又被当做input，attachment的布局要求更严格
				auto& renderPassDesc = renderPassDescs[task.index];
				for (auto colorAttachment : renderPassDesc.colorAttachments)
				{
					auto attachmentImageView = GetResolvedImageView(taskIndex, colorAttachment.imageViewProxyId);
					if (ImageViewContainsSubresource(attachmentImageView, imageData, mipLevel, arrayLayer))
						return ImageUsageTypes::ColorAttachment;
				}
				if (!(renderPassDesc.depthAttachment.imageViewProxyId == ImageViewProxyId()))
				{
					auto attachmentImageView = GetResolvedImageView(taskIndex, renderPassDesc.depthAttachment.imageViewProxyId);
					if (ImageViewContainsSubresource(attachmentImageView, imageData, mipLevel, arrayLayer))
						return ImageUsageTypes::DepthAttachment;
				}
				for (auto imageViewProxy : renderPassDesc.inputImageViewProxies)
				{
					if (ImageViewContainsSubresource(GetResolvedImageView(taskIndex, imageViewProxy), imageData, mipLevel, arrayLayer))
						return ImageUsageTypes::GraphicsShaderRead;
				}
				for (auto imageViewProxy : renderPassDesc.inoutStorageImageProxies)
				{
					if (ImageViewContainsSubresource(GetResolvedImageView(taskIndex, imageViewProxy), imageData, mipLevel, arrayLayer))
						return ImageUsageTypes::GraphicsShaderReadWrite;
				}
			}
			break;
		case Task::Types::ComputePass:
			{
				auto& computePassDesc = computePassDescs[task.index];
				for (auto imageViewProxy : computePassDesc.inputImageViewProxies)
				{
					if (ImageViewContainsSubresource(GetResolvedImageView(taskIndex, imageViewProxy), imageData, mipLevel, arrayLayer))
						return ImageUsageTypes::ComputeShaderRead;
				}
				for (auto imageViewProxy : computePassDesc.inoutStorageImageProxies)
				{
					if (ImageViewContainsSubresource(GetResolvedImageView(taskIndex, imageViewProxy), imageData, mipLevel, arrayLayer))
						return ImageUsageTypes::ComputeShaderReadWrite;
				}
			}
			break;
		case Task::Types::TransferPass:
			{
				auto& transferPassDesc = transferPassDescs[task.index];
				for (auto srcImageViewProxy : transferPassDesc.srcImageViewProxies)
				{
					if (ImageViewContainsSubresource(GetResolvedImageView(taskIndex, srcImageViewProxy), imageData, mipLevel, arrayLayer)) return ImageUsageTypes::TransferSrc;
				}
				for (auto dstImageViewProxy : transferPassDesc.dstImageViewProxies)
				{
					if (ImageViewContainsSubresource(GetResolvedImageView(taskIndex, dstImageViewProxy), imageData, mipLevel, arrayLayer)) return ImageUsageTypes::TransferDst;
				}
			}
			break;
		case Task::Types::ImagePresent:
			{
				auto& imagePresentDesc = imagePresentDescs[task.index];
				if (ImageViewContainsSubresource(GetResolvedImageView(taskIndex, imagePresentDesc.presentImageViewProxyId), imageData, mipLevel, arrayLayer)) return ImageUsageTypes::Present;
			}
			break;
		default:
			{
			}
		}
		return ImageUsageTypes::None;
	}

	BufferUsageTypes RenderGraph::GetTaskBufferUsageType(size_t taskIndex, Buffer* buffer)
	{
		// 和上面函数 类似的思路
		Task& task = tasks[taskIndex];
		switch (task.type)
		{
		case Task::Types::RenderPass:
			{
				auto& renderPassDesc = renderPassDescs[task.index];
				for (auto storageBufferProxy : renderPassDesc.inoutStorageBufferProxies)
				{
					auto storageBuffer = GetResolvedBuffer(taskIndex, storageBufferProxy);
					if (buffer->GetHandle() == storageBuffer->GetHandle())
						return BufferUsageTypes::GraphicsShaderReadWrite;
				}
				for (auto vertexBufferProxy : renderPassDesc.vertexBufferProxies)
				{
					auto vertexBuffer = GetResolvedBuffer(taskIndex, vertexBufferProxy);
					if (buffer->GetHandle() == vertexBuffer->GetHandle())
						return BufferUsageTypes::VertexBuffer;
				}
			}
			break;
		case Task::Types::ComputePass:
			{
				auto& computePassDesc = computePassDescs[task.index];
				for (auto storageBufferProxy : computePassDesc.inoutStorageBufferProxies)
				{
					auto storageBuffer = GetResolvedBuffer(taskIndex, storageBufferProxy);
					if (buffer->GetHandle() == storageBuffer->GetHandle())
						return BufferUsageTypes::ComputeShaderReadWrite;
				}
			}
			break;

		case Task::Types::TransferPass:
			{
				auto& transferPassDesc = transferPassDescs[task.index];
				for (auto srcBufferProxy : transferPassDesc.srcBufferProxies)
				{
					auto srcBuffer = GetResolvedBuffer(taskIndex, srcBufferProxy);
					if (buffer->GetHandle() == srcBuffer->GetHandle())
						return BufferUsageTypes::TransferSrc;
				}
				for (auto dstBufferProxy : transferPassDesc.dstBufferProxies)
				{
					auto dstBuffer = GetResolvedBuffer(taskIndex, dstBufferProxy);
					// legit vulkan是src，感觉写错了
					if (buffer->GetHandle() == dstBuffer->GetHandle())
						return BufferUsageTypes::TransferDst;
				}
			}
			break;
		default:
			{
			}
		}
		return BufferUsageTypes::None;
	}

	/*
	 * 从当前task一直往前找，找到某一个资源上一次被使用时候的用途类型，这个信息就是barrier的源状态
	 * 两层fallback，首先在当前task去找，然后再寻找外部图像的声明用途，外部传入的图像在进入render graph之前有一个已知状态
	 */
	ImageUsageTypes RenderGraph::GetLastImageSubresourceUsageType(size_t taskIndex, ImageData* imageData, uint32_t mipLevel, uint32_t arrayLayer)
	{
		for (size_t taskOffset = 0; taskOffset < taskIndex; taskOffset++)
		{
			size_t prevTaskIndex = taskIndex - taskOffset - 1;
			auto usageType = GetTaskImageSubresourceUsageType(prevTaskIndex, imageData, mipLevel, arrayLayer);
			if (usageType != ImageUsageTypes::None) return usageType;
		}

		for (auto& imageViewProxy : imageViewProxies)
		{
			if (imageViewProxy.type == ImageViewProxy::Types::External && imageViewProxy.externalView->GetImageData() == imageData)
			{
				return imageViewProxy.externalUsageType;
			}
		}
		return ImageUsageTypes::None;
	}

	BufferUsageTypes RenderGraph::GetLastBufferUsageType(size_t taskIndex, Buffer* buffer)
	{
		// legit vulkan中从1开始，但是感觉很微妙，所以改成了从0开始，之前的默认task从frame sync begin开始
		for (size_t taskOffset = 0; taskOffset < taskIndex; taskOffset++)
		{
			size_t prevTaskIndex = taskIndex - taskOffset;
			auto usageType = GetTaskBufferUsageType(prevTaskIndex, buffer);
			if (usageType != BufferUsageTypes::None) return usageType;
		}
		return BufferUsageTypes::None;
	}

	// 开始构造vulkan barrier对象
	void RenderGraph::FlushImageTransitionBarriers(ImageData* imageData, vk::ImageSubresourceRange range, ImageUsageTypes srcUsageType, ImageUsageTypes dstUsageType, vk::PipelineStageFlags& srcStage, vk::PipelineStageFlags& dstStage, std::vector<vk::ImageMemoryBarrier>& imageBarriers)
	{
		if (IsImageBarrierNeeded(srcUsageType, dstUsageType) &&
			//srcUsageType != ImageUsageTypes::ColorAttachment && //this is done automatically when constructing render pass
			//srcUsageType != ImageUsageTypes::DepthAttachment &&
			range.layerCount > 0 && range.levelCount > 0) //this is done automatically when constructing render pass
		{
			auto srcImageAccessPattern = GetSrcImageAccessPattern(srcUsageType);
			auto dstImageAccessPattern = GetDstImageAccessPattern(dstUsageType);
			// src的布局，类型 dst的布局，mip layer
			auto imageBarrier = vk::ImageMemoryBarrier().setSrcAccessMask(srcImageAccessPattern.accessMask).setOldLayout(srcImageAccessPattern.layout).setDstAccessMask(dstImageAccessPattern.accessMask).setNewLayout(dstImageAccessPattern.layout).setSubresourceRange(range).setImage(imageData->GetHandle());

			if (srcImageAccessPattern.queueFamilyType == dstImageAccessPattern.queueFamilyType)
			{
				imageBarrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED).setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
			}
			else
			{
				imageBarrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED).setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
			}
			srcStage |= srcImageAccessPattern.stage;
			dstStage |= dstImageAccessPattern.stage;
			imageBarriers.push_back(imageBarrier);
		}
	}

	void RenderGraph::AddImageTransitionBarriers(ImageView* imageView, ImageUsageTypes dstUsageType, size_t dstTaskIndex, vk::PipelineStageFlags& srcStage, vk::PipelineStageFlags& dstStage, std::vector<vk::ImageMemoryBarrier>& imageBarriers)
	{
		auto range = vk::ImageSubresourceRange().setAspectMask(imageView->GetImageData()->GetAspectFlags());

		for (uint32_t arrayLayer = imageView->GetBaseArrayLayer(); arrayLayer < imageView->GetBaseArrayLayer() + imageView->GetArrayLayersCount(); arrayLayer++)
		{
			range.setBaseArrayLayer(arrayLayer).setLayerCount(1).setBaseMipLevel(imageView->GetBaseMipLevel()).setLevelCount(0);
			auto prevSubresourceUsageType = ImageUsageTypes::None;

			for (uint32_t mipLevel = imageView->GetBaseMipLevel(); mipLevel < imageView->GetBaseMipLevel() + imageView->GetMipLevelsCount(); mipLevel++)
			{
				auto lastUsageType = GetLastImageSubresourceUsageType(dstTaskIndex, imageView->GetImageData(), mipLevel, arrayLayer);
				if (prevSubresourceUsageType != lastUsageType)
				{
					FlushImageTransitionBarriers(imageView->GetImageData(), range, prevSubresourceUsageType, dstUsageType, srcStage, dstStage, imageBarriers);
					range.setBaseMipLevel(mipLevel).setLevelCount(0);
					prevSubresourceUsageType = lastUsageType;
				}
				range.levelCount++;
			}
			FlushImageTransitionBarriers(imageView->GetImageData(), range, prevSubresourceUsageType, dstUsageType, srcStage, dstStage, imageBarriers);
		}
	}

	void RenderGraph::FlushBufferTransitionBarriers(Buffer* buffer, BufferUsageTypes srcUsageType, BufferUsageTypes dstUsageType, vk::PipelineStageFlags& srcStage, vk::PipelineStageFlags& dstStage, std::vector<vk::BufferMemoryBarrier>& bufferBarriers)
	{
		if (IsBufferBarrierNeeded(srcUsageType, dstUsageType))
		{
			auto srcBufferAccessPattern = GetSrcBufferAccessPattern(srcUsageType);
			auto dstBufferAccessPattern = GetDstBufferAccessPattern(dstUsageType);
			auto bufferBarrier = vk::BufferMemoryBarrier().setSrcAccessMask(srcBufferAccessPattern.accessMask).setOffset(0).setSize(VK_WHOLE_SIZE).setDstAccessMask(dstBufferAccessPattern.accessMask).setBuffer(buffer->GetHandle());

			if (srcBufferAccessPattern.queueFamilyType == dstBufferAccessPattern.queueFamilyType)
			{
				bufferBarrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED).setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
			}
			else
			{
				bufferBarrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED).setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
			}
			srcStage |= srcBufferAccessPattern.stage;
			dstStage |= dstBufferAccessPattern.stage;
			bufferBarriers.push_back(bufferBarrier);
		}
	}

	void RenderGraph::AddBufferBarriers(Buffer* buffer, BufferUsageTypes dstUsageType, size_t dstTaskIndex, vk::PipelineStageFlags& srcStage, vk::PipelineStageFlags& dstStage, std::vector<vk::BufferMemoryBarrier>& bufferBarriers)
	{
		auto lastUsageType = GetLastBufferUsageType(dstTaskIndex, buffer);
		FlushBufferTransitionBarriers(buffer, lastUsageType, dstUsageType, srcStage, dstStage, bufferBarriers);
	}
}
