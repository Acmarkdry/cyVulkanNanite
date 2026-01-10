#pragma once
#include <string>

#include "Pool.h"
#include "RenderGraph.h"
#include "Synchronization.h"

namespace cyRenderGraph
{
	class RenderGraph;

	// ImageProxy 已在 RenderGraph.h 中定义
	struct ImageProxy;
	using ImageProxyPool = Pool<ImageProxy>;

	struct ImageViewProxy;
	using ImageViewProxyPool = Pool<ImageViewProxy>;

	struct BufferProxy;
	using BufferProxyPool = Pool<BufferProxy>;

	using ImageProxyId = ImageProxyPool::ID;
	using ImageViewProxyId = ImageViewProxyPool::ID;
	using BufferProxyId = BufferProxyPool::ID;

	struct BufferProxy
	{
		enum struct Types
		{
			Transient,
			External
		  };

		BufferCache::BufferKey bufferKey;
		Buffer *externalBuffer;

		Buffer *resolvedBuffer;

		Types type;
	};

	// ImageProxy 定义在命名空间级别，供 Pool<ImageProxy> 使用
	struct ImageProxy
	{
		enum struct Types
		{
			Transient,
			External
		};

		ImageKey imageKey;
		ImageData *externalImage;
		ImageData *resolvedImage;
		Types type;
	};

	struct ImageViewProxy
	{
		enum struct Types
		{
			Transient,
			External
		  };
		bool Contains(const ImageViewProxy &other)
		{
			if (type == Types::Transient && subresourceRange.Contains(other.subresourceRange) && type == other.type && imageProxyId == other.imageProxyId)
			{
				return true;
			}

			if (type == Types::External && externalView == other.externalView)
			{
				return true;
			}
			return false;
		}
		ImageProxyId imageProxyId;
		ImageSubresourceRange subresourceRange;

		ImageView *externalView;
		ImageUsageTypes externalUsageType;

		ImageView *resolvedImageView;
		std::string debugName;

		Types type;
	};

	struct ImageHandleInfo
	{
		ImageHandleInfo()
		{
		}

		ImageHandleInfo(RenderGraph* renderGraph, ImageProxyId imageProxyId)
		{
			this->renderGraph = renderGraph;
			this->imageProxyId = imageProxyId;
		}

		void Reset()
		{
			renderGraph->DeleteImage(imageProxyId);
		}

		ImageProxyId Id() const
		{
			return imageProxyId;
		}

		void SetDebugName(std::string name) const
		{
			renderGraph->SetImageProxyDebugName(imageProxyId, name);
		}

	private:
		RenderGraph* renderGraph;
		ImageProxyId imageProxyId;
		friend class RenderGraph;
	};

	struct ImageViewHandleInfo
	{
		ImageViewHandleInfo()
		{
		}

		ImageViewHandleInfo(RenderGraph* renderGraph, ImageViewProxyId imageViewProxyId)
		{
			this->renderGraph = renderGraph;
			this->imageViewProxyId = imageViewProxyId;
		}

		void Reset()
		{
			renderGraph->DeleteImageView(imageViewProxyId);
		}

		ImageViewProxyId Id() const
		{
			return imageViewProxyId;
		}

		void SetDebugName(std::string name) const
		{
			renderGraph->SetImageViewProxyDebugName(imageViewProxyId, name);
		}

	private:
		RenderGraph* renderGraph;
		ImageViewProxyId imageViewProxyId;
		friend class RenderGraph;
	};

	struct BufferHandleInfo
	{
		BufferHandleInfo()
		{
		}

		BufferHandleInfo(RenderGraph* renderGraph, BufferProxyId bufferProxyId)
		{
			this->renderGraph = renderGraph;
			this->bufferProxyId = bufferProxyId;
		}

		void Reset()
		{
			renderGraph->DeleteBuffer(bufferProxyId);
		}

		BufferProxyId Id() const
		{
			return bufferProxyId;
		}

	private:
		RenderGraph* renderGraph;
		BufferProxyId bufferProxyId;
		friend class RenderGraph;
	};
}
