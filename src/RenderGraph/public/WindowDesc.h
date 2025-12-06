#pragma once
#include <vulkan/vulkan.hpp>

namespace cyRenderGraph
{
	struct WindowDesc
	{
#if defined(WIN32)
		HINSTANCE hInstance;
		HWND hWnd;
#else
		wl_display *display;
		wl_surface *surface;
#endif
	};
}
