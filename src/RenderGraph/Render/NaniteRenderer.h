#pragma once
#include "BaseVulkanRender.h"

namespace cyRenderGraph
{
	class NaniteRenderer:public BaseVulkanRenderer
	{
	public:
		NaniteRenderer(Core* core)
		{
			this->core = core;
		}

	private:
		Core* core;
		
	};
	
}

