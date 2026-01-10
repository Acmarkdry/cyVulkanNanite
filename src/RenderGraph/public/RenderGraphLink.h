#pragma once
#include <glm/vec2.hpp>

#include "DescriptorSetCache.h"
#include "Image.h"
#include "PipelineCache.h"
#include "RenderGraphUtils.h"
#include "WindowDesc.h"


namespace cyRenderGraph
{
	class Swapchain;
	class RenderGraph;

	class Core
	{
	public:
		Core(const char** instanceExtensions, uint32_t instanceExtensionsCount, WindowDesc* compatibleWindowDesc, bool enableDebugging);

		~Core();
		void ClearCaches();
		std::unique_ptr<Swapchain> CreateSwapchain(WindowDesc windowDesc, glm::uvec2 defaultSize, uint32_t imagesCount, vk::PresentModeKHR preferredMode);


		template <typename Handle, typename Loader>
		static void SetObjectDebugName(vk::Device logicalDevice, Loader& loader, Handle objHandle, std::string name)
		{
			auto nameInfo = vk::DebugUtilsObjectNameInfoEXT()
			                .setObjectHandle(reinterpret_cast<uint64_t>(typename Handle::CType(objHandle)))
			                .setObjectType(objHandle.objectType)
			                .setPObjectName(name.c_str());
			if (loader.vkSetDebugUtilsObjectNameEXT)
				auto res = logicalDevice.setDebugUtilsObjectNameEXT(&nameInfo, loader);
		}

		template <typename T>
		void SetObjectDebugName(T objHandle, std::string name)
		{
			SetObjectDebugName(logicalDevice.get(), loader, objHandle, name);
		}

		void SetDebugName(ImageData* imageData, std::string name);

		vk::CommandPool GetCommandPool();
		vk::Device GetLogicalDevice();
		vk::PhysicalDevice GetPhysicalDevice();
		RenderGraph* GetRenderGraph();
		std::vector<vk::UniqueCommandBuffer> AllocateCommandBuffers(size_t count);
		vk::UniqueSemaphore CreateVulkanSemaphore();
		vk::UniqueFence CreateFence(bool state);
		void WaitForFence(vk::Fence fence);
		void ResetFence(vk::Fence fence);
		void WaitIdle();
		vk::Queue GetGraphicsQueue();
		vk::Queue GetPresentQueue();
		uint32_t GetDynamicMemoryAlignment();
		DescriptorSetCache* GetDescriptorSetCache();
		PipelineCache* GetPipelineCache();

	private:
		static vk::UniqueInstance CreateInstance(const std::vector<const char*>& instanceExtensions, const std::vector<const char*>& validationLayers);

		static vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::detail::DispatchLoaderDynamic> CreateDebugUtilsMessenger(vk::Instance instance, vk::PFN_DebugUtilsMessengerCallbackEXT debugCallback, vk::detail::DispatchLoaderDynamic& loader);

		static vk::Bool32 DebugMessageCallback(
			vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
			const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData);

		friend class Swapchain;
		static vk::PhysicalDevice FindPhysicalDevice(vk::Instance instance);

		static QueueFamilyIndices FindQueueFamilyIndices(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface);

		static vk::UniqueDevice CreateLogicalDevice(vk::PhysicalDevice physicalDevice, QueueFamilyIndices familyIndices, std::vector<const char*> deviceExtensions, std::vector<const char*> validationLayers);
		static vk::Queue GetDeviceQueue(vk::Device logicalDevice, uint32_t queueFamilyIndex);
		static vk::UniqueCommandPool CreateCommandPool(vk::Device logicalDevice, uint32_t familyIndex);

		vk::UniqueInstance instance;
		vk::detail::DispatchLoaderDynamic loader;
		vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::detail::DispatchLoaderDynamic> debugUtilsMessenger;
		vk::PhysicalDevice physicalDevice;
		vk::UniqueDevice logicalDevice;
		vk::UniqueCommandPool commandPool;
		vk::Queue graphicsQueue;
		vk::Queue presentQueue;

		std::unique_ptr<DescriptorSetCache> descriptorSetCache;
		std::unique_ptr<PipelineCache> pipelineCache;
		std::unique_ptr<RenderGraph> renderGraph;


		QueueFamilyIndices queueFamilyIndices;
	};
}
