#pragma once
#include <vulkan/vulkan.hpp>

#include "WindowDesc.h"

namespace cyRenderGraph
{
#if defined(WIN32)
	static vk::UniqueSurfaceKHR CreateSurface(vk::Instance instance, WindowDesc desc)
	{
		auto surfaceCreateInfo = vk::Win32SurfaceCreateInfoKHR()
		  .setHwnd(desc.hWnd)
		  .setHinstance(desc.hInstance);

		return instance.createWin32SurfaceKHRUnique(surfaceCreateInfo);

	}
#else
	static vk::UniqueSurfaceKHR CreateSurface(vk::Instance instance, WindowDesc desc)
	{
		auto surfaceCreateInfo = vk::WaylandSurfaceCreateInfoKHR()
		  .setDisplay(desc.display)
		  .setSurface(desc.surface);

		return instance.createWaylandSurfaceKHRUnique(surfaceCreateInfo);

	}
#endif
	
};
