#pragma once
#include "Scene.h"

namespace cyRenderGraph
{
	class BaseVulkanRenderer
	{
	public:
		BaseVulkanRenderer(){}
		virtual ~BaseVulkanRenderer() {}
		virtual void RecreateSceneResources(Scene *scene){}
		virtual void RecreateSwapchainResources(vk::Extent2D viewportExtent, size_t inFlightFramesCount){}
		virtual void RenderFrame(const InFlightQueue::FrameInfo &frameInfo, const Camera &camera, const Camera &light, Scene *scene, GLFWwindow *window){}
		virtual void ReloadShaders(){}
		virtual void ChangeView(){}
	};
	
}
