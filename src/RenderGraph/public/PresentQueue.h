#pragma once
#include <glm/vec2.hpp>

#include "ImageView.h"
#include "Profiler.h"
#include "RenderGraphLink.h"
#include "ShaderMemoryPool.h"
#include "Swapchain.h"
#include "WindowDesc.h"

namespace cyRenderGraph
{
	// swap chain处理
	struct PresentQueue
	{
		PresentQueue(Core* core, WindowDesc windowDesc, glm::uvec2 defaultSize, uint32_t imagesCount, vk::PresentModeKHR preferredMode);
		ImageView* AcquireImage(vk::Semaphore signalSemaphore);
		void PresentImage(vk::Semaphore waitSemaphore);
		vk::Extent2D GetImageSize();

	private:
		Core* core;
		uint32_t imageIndex;
		vk::Rect2D swapchainRect;

		std::unique_ptr<Swapchain> swapchain;
		std::vector<ImageView*> swapchainImageViews;
	};

	struct InFlightQueue
	{
		InFlightQueue(Core* core, WindowDesc windowDesc, glm::uvec2 defaultSize, uint32_t inFlightCount, vk::PresentModeKHR preferredMode);

		vk::Extent2D GetImageSize();
		size_t GetInFlightFramesCount();

		struct FrameInfo
		{
			ShaderMemoryPool* memoryPool;
			size_t frameIndex;
			RenderGraph::ImageViewProxyId swapchainImageViewProxyId;
		};

		FrameInfo BeginFrame();
		void EndFrame();

		const std::vector<ProfilerTask>& GetLastFrameCpuProfilerData();
		const std::vector<ProfilerTask>& GetLastFrameGpuProfilerData();
		CpuProfiler& GetCpuProfiler();

	private:
		std::unique_ptr<ShaderMemoryPool> memoryPool;
		std::unique_ptr<PresentQueue> presentQueue;
		std::map<ImageView*, RenderGraph::ImageViewProxyUnique> swapchainImageViewProxies;

		struct FrameResources
		{
			vk::UniqueSemaphore imageAcquiredSemaphore;
			vk::UniqueSemaphore renderingFinishedSemaphore;
			vk::UniqueFence inFlightFence;

			vk::UniqueCommandBuffer commandBuffer;
			std::unique_ptr<Buffer> shaderMemoryBuffer;
			std::unique_ptr<GpuProfiler> gpuProfiler;
		};

		std::vector<FrameResources> frames;
		size_t frameIndex;

		Core* core;
		ImageView* currSwapchainImageView;
		CpuProfiler cpuProfiler;
		std::vector<ProfilerTask> lastFrameCpuProfilerTasks;

		size_t profilerFrameId;
	};

	struct ExecuteOnceQueue
	{
		ExecuteOnceQueue(Core* core);
		vk::CommandBuffer BeginCommandBuffer();
		void EndCommandBuffer();

	private:
		Core* core;
		vk::UniqueCommandBuffer commandBuffer;
	};
}
