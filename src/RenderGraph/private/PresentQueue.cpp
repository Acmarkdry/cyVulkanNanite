#include "PresentQueue.h"

namespace cyRenderGraph
{
	// PresentQueue implementations
	PresentQueue::PresentQueue(Core *core, WindowDesc windowDesc, glm::uvec2 defaultSize, uint32_t imagesCount, vk::PresentModeKHR preferredMode)
	{
		this->core = core;
		this->swapchain = core->CreateSwapchain(windowDesc, defaultSize, imagesCount, preferredMode);
		this->swapchainImageViews = swapchain->GetImageViews();
		this->swapchainRect = vk::Rect2D(vk::Offset2D(), swapchain->GetSize());
		this->imageIndex = -1;
	}

	cyRenderGraph::ImageView *PresentQueue::AcquireImage(vk::Semaphore signalSemaphore)
	{
		this->imageIndex = swapchain->AcquireNextImage(signalSemaphore).value;
		return swapchainImageViews[imageIndex];
	}

	void PresentQueue::PresentImage(vk::Semaphore waitSemaphore)
	{
		vk::SwapchainKHR swapchains[] = { swapchain->GetHandle() };
		vk::Semaphore waitSemaphores[] = { waitSemaphore };
		auto presentInfo = vk::PresentInfoKHR()
			.setSwapchainCount(1)
			.setPSwapchains(swapchains)
			.setPImageIndices(&imageIndex)
			.setPResults(nullptr)
			.setWaitSemaphoreCount(1)
			.setPWaitSemaphores(waitSemaphores);

		auto res = core->GetPresentQueue().presentKHR(presentInfo);
	}

	vk::Extent2D PresentQueue::GetImageSize()
	{
		return swapchain->GetSize();
	}

	// InFlightQueue implementations
	InFlightQueue::InFlightQueue(Core *core, WindowDesc windowDesc, glm::uvec2 defaultSize, uint32_t inFlightCount, vk::PresentModeKHR preferredMode)
	{
		this->core = core;
		this->memoryPool = std::make_unique<ShaderMemoryPool>(core->GetDynamicMemoryAlignment());

		presentQueue.reset(new PresentQueue(core, windowDesc, defaultSize, inFlightCount, preferredMode));

		for (size_t frameIndex = 0; frameIndex < inFlightCount; frameIndex++)
		{
			FrameResources frame;
			frame.inFlightFence = core->CreateFence(true);
			frame.imageAcquiredSemaphore = core->CreateVulkanSemaphore();
			frame.renderingFinishedSemaphore = core->CreateVulkanSemaphore();

			frame.commandBuffer = std::move(core->AllocateCommandBuffers(1)[0]);
			core->SetObjectDebugName(frame.commandBuffer.get(), std::string("Frame") + std::to_string(frameIndex) + " command buffer");
			frame.shaderMemoryBuffer = std::unique_ptr<Buffer>(new Buffer(core->GetPhysicalDevice(), core->GetLogicalDevice(), 100000000, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent));
			frame.gpuProfiler = std::unique_ptr<GpuProfiler>(new GpuProfiler(core->GetPhysicalDevice(), core->GetLogicalDevice(), 4096));
			frames.push_back(std::move(frame));
		}
		frameIndex = 0;
	}

	vk::Extent2D InFlightQueue::GetImageSize()
	{
		return presentQueue->GetImageSize();
	}

	size_t InFlightQueue::GetInFlightFramesCount()
	{
		return frames.size();
	}

	InFlightQueue::FrameInfo InFlightQueue::BeginFrame()
	{
		this->profilerFrameId = cpuProfiler.StartFrame();

		auto &currFrame = frames[frameIndex];
		{
			auto fenceTask = cpuProfiler.StartScopedTask("WaitForFence", Colors::pomegranate);
			core->WaitForFence(currFrame.inFlightFence.get());
			core->ResetFence(currFrame.inFlightFence.get());
		}

		{
			auto imageAcquireTask = cpuProfiler.StartScopedTask("ImageAcquire", Colors::emerald);
			currSwapchainImageView = presentQueue->AcquireImage(currFrame.imageAcquiredSemaphore.get());
		}

		{
			auto gpuGatheringTask = cpuProfiler.StartScopedTask("GpuPrfGathering", Colors::amethyst);
			currFrame.gpuProfiler->GatherTimestamps();
		}

		auto &swapchainViewProxyId = swapchainImageViewProxies[currSwapchainImageView];
		if (!swapchainViewProxyId.IsAttached())
		{
			swapchainViewProxyId = core->GetRenderGraph()->AddExternalImageView(currSwapchainImageView);
		}
		core->GetRenderGraph()->AddPass(RenderGraph::FrameSyncBeginPassDesc());

		memoryPool->MapBuffer(currFrame.shaderMemoryBuffer.get());

		FrameInfo frameInfo;
		frameInfo.memoryPool = memoryPool.get();
		frameInfo.frameIndex = frameIndex;
		frameInfo.swapchainImageViewProxyId = swapchainViewProxyId->Id();

		return frameInfo;
	}

	void InFlightQueue::EndFrame()
	{
		auto &currFrame = frames[frameIndex];

		core->GetRenderGraph()->AddImagePresent(swapchainImageViewProxies[currSwapchainImageView]->Id());
		core->GetRenderGraph()->AddPass(RenderGraph::FrameSyncEndPassDesc());

		auto bufferBeginInfo = vk::CommandBufferBeginInfo()
			.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
		currFrame.commandBuffer->begin(bufferBeginInfo);
		{
			auto gpuFrame = currFrame.gpuProfiler->StartScopedFrame(currFrame.commandBuffer.get());
			core->GetRenderGraph()->Execute(currFrame.commandBuffer.get(), &cpuProfiler, currFrame.gpuProfiler.get());
		}
		currFrame.commandBuffer->end();

		memoryPool->UnmapBuffer();

		{
			{
				auto presentTask = cpuProfiler.StartScopedTask("Submit", Colors::amethyst);
				vk::Semaphore waitSemaphores[] = { currFrame.imageAcquiredSemaphore.get() };
				vk::Semaphore signalSemaphores[] = { currFrame.renderingFinishedSemaphore.get() };
				vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

				auto submitInfo = vk::SubmitInfo()
					.setWaitSemaphoreCount(1)
					.setPWaitSemaphores(waitSemaphores)
					.setPWaitDstStageMask(waitStages)
					.setCommandBufferCount(1)
					.setPCommandBuffers(&currFrame.commandBuffer.get())
					.setSignalSemaphoreCount(1)
					.setPSignalSemaphores(signalSemaphores);

				core->GetGraphicsQueue().submit({ submitInfo }, currFrame.inFlightFence.get());
			}
			auto presentTask = cpuProfiler.StartScopedTask("Present", Colors::alizarin);
			presentQueue->PresentImage(currFrame.renderingFinishedSemaphore.get());
		}
		frameIndex = (frameIndex + 1) % frames.size();

		cpuProfiler.EndFrame(profilerFrameId);
		lastFrameCpuProfilerTasks = cpuProfiler.GetProfilerTasks();
	}

	const std::vector<ProfilerTask> &InFlightQueue::GetLastFrameCpuProfilerData()
	{
		return lastFrameCpuProfilerTasks;
	}

	const std::vector<ProfilerTask> &InFlightQueue::GetLastFrameGpuProfilerData()
	{
		return frames[frameIndex].gpuProfiler->GetProfilerTasks();
	}

	CpuProfiler &InFlightQueue::GetCpuProfiler()
	{
		return cpuProfiler;
	}

	// ExecuteOnceQueue implementations
	ExecuteOnceQueue::ExecuteOnceQueue(Core *core)
	{
		this->core = core;
		commandBuffer = std::move(core->AllocateCommandBuffers(1)[0]);
	}

	vk::CommandBuffer ExecuteOnceQueue::BeginCommandBuffer()
	{
		auto bufferBeginInfo = vk::CommandBufferBeginInfo()
			.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		commandBuffer->begin(bufferBeginInfo);
		return commandBuffer.get();
	}

	void ExecuteOnceQueue::EndCommandBuffer()
	{
		commandBuffer->end();
		vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eAllCommands };

		auto submitInfo = vk::SubmitInfo()
			.setWaitSemaphoreCount(0)
			.setPWaitDstStageMask(waitStages)
			.setCommandBufferCount(1)
			.setPCommandBuffers(&commandBuffer.get())
			.setSignalSemaphoreCount(0);

		core->GetGraphicsQueue().submit({ submitInfo }, nullptr);
		core->GetGraphicsQueue().waitIdle();
	}
}
