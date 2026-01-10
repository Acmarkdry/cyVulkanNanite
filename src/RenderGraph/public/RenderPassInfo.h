#pragma once
#include "ResourceProxy.h"

namespace cyRenderGraph
{
	struct PassContext
	{
		ImageView* GetImageView(ImageViewProxyId imageViewProxyId)
		{
			return resolvedImageViews[imageViewProxyId.asInt];
		}

		Buffer* GetBuffer(BufferProxyId bufferProxy)
		{
			return resolvedBuffers[bufferProxy.asInt];
		}

		vk::CommandBuffer GetCommandBuffer()
		{
			return commandBuffer;
		}

	private:
		std::vector<ImageView*> resolvedImageViews;
		std::vector<Buffer*> resolvedBuffers;
		vk::CommandBuffer commandBuffer;
		friend class RenderGraph;
	};

	struct RenderPassContext : PassContext
	{
		RenderPass* GetRenderPass()
		{
			return renderPass;
		}

	private:
		RenderPass* renderPass;
		friend class RenderGraph;
	};
	
	/*
	 * 普通的vs + fg
	 */
	struct RenderPassDesc
	{
		struct Attachment
		{
			ImageViewProxyId imageViewProxyId;
			vk::AttachmentLoadOp loadOp;
			vk::ClearValue clearValue;
		};
		
		RenderPassDesc()
		{
			profilerTaskName = "RenderPass";
	        profilerTaskColor = glm::packUnorm4x8(glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
		}
		
      RenderPassDesc &SetColorAttachments(
        const std::vector<ImageViewProxyId> &_colorAttachmentViewProxies, 
        vk::AttachmentLoadOp _loadOp = vk::AttachmentLoadOp::eDontCare, 
        vk::ClearValue _clearValue = vk::ClearColorValue(std::array<float, 4>{1.0f, 0.5f, 0.0f, 1.0f}))
      {
        this->colorAttachments.resize(_colorAttachmentViewProxies.size());
        for (size_t index = 0; index < _colorAttachmentViewProxies.size(); index++)
        {
          this->colorAttachments[index] = { _colorAttachmentViewProxies [index], _loadOp, _clearValue};
        }
        return *this;
      }
      RenderPassDesc &SetColorAttachments(std::vector<Attachment> &&_colorAttachments)
      {
        this->colorAttachments = std::move(_colorAttachments);
        return *this;
      }

      RenderPassDesc &SetDepthAttachment(
        ImageViewProxyId _depthAttachmentViewProxyId,
        vk::AttachmentLoadOp _loadOp = vk::AttachmentLoadOp::eDontCare,
        vk::ClearValue _clearValue = vk::ClearDepthStencilValue(1.0f, 0))
      {
        this->depthAttachment.imageViewProxyId = _depthAttachmentViewProxyId;
        this->depthAttachment.loadOp = _loadOp;
        this->depthAttachment.clearValue = _clearValue;
        return *this;
      }
      RenderPassDesc &SetDepthAttachment(Attachment _depthAttachment)
      {
        this->depthAttachment = _depthAttachment;
        return *this;
      }
      RenderPassDesc& SetVertexBuffers(std::vector<BufferProxyId>&& _vertexBufferProxies)
      {
        this->vertexBufferProxies = std::move(_vertexBufferProxies);
        return *this;
      }

      RenderPassDesc &SetInputImages(std::vector<ImageViewProxyId> &&_inputImageViewProxies)
      {
        this->inputImageViewProxies = std::move(_inputImageViewProxies);
        return *this;
      }
      RenderPassDesc &SetStorageBuffers(std::vector<BufferProxyId> &&_inoutStorageBufferProxies)
      {
        this->inoutStorageBufferProxies = _inoutStorageBufferProxies;
        return *this;
      }
      RenderPassDesc &SetStorageImages(std::vector<ImageViewProxyId> &&_inoutStorageImageProxies)
      {
        this->inoutStorageImageProxies = _inoutStorageImageProxies;
        return *this;
      }
      RenderPassDesc &SetRenderAreaExtent(vk::Extent2D _renderAreaExtent)
      {
        this->renderAreaExtent = _renderAreaExtent;
        return *this;
      }

      RenderPassDesc &SetRecordFunc(std::function<void(RenderPassContext)> _recordFunc)
      {
        this->recordFunc = _recordFunc;
        return *this;
      }
      RenderPassDesc &SetProfilerInfo(uint32_t taskColor, std::string taskName)
      {
        this->profilerTaskColor = taskColor;
        this->profilerTaskName = taskName;
        return *this;
      }
	
	
		std::vector<Attachment> colorAttachments;
		Attachment depthAttachment;
		std::vector<ImageViewProxyId> inputImageViewProxies;
		std::vector<BufferProxyId> vertexBufferProxies;
		std::vector<BufferProxyId> inoutStorageBufferProxies;
		std::vector<ImageViewProxyId> inoutStorageImageProxies;

		vk::Extent2D renderAreaExtent;
		std::function<void(RenderPassContext)> recordFunc;
		
		std::string profilerTaskName;
		uint32_t profilerTaskColor;
	};
	
	struct ComputePassDesc
	{
		ComputePassDesc()
		{
			profilerTaskName = "ComputePass";
			profilerTaskColor = legit::Colors::belizeHole;
		}
		ComputePassDesc &SetInputImages(std::vector<ImageViewProxyId> &&_inputImageViewProxies)
		{
			this->inputImageViewProxies = std::move(_inputImageViewProxies);
			return *this;
		}
		ComputePassDesc &SetStorageBuffers(std::vector<BufferProxyId> &&_inoutStorageBufferProxies)
		{
			this->inoutStorageBufferProxies = _inoutStorageBufferProxies;
			return *this;
		}
		ComputePassDesc &SetStorageImages(std::vector<ImageViewProxyId> &&_inoutStorageImageProxies)
		{
			this->inoutStorageImageProxies = _inoutStorageImageProxies;
			return *this;
		}
		ComputePassDesc &SetRecordFunc(std::function<void(PassContext)> _recordFunc)
		{
			this->recordFunc = _recordFunc;
			return *this;
		}
		ComputePassDesc &SetProfilerInfo(uint32_t taskColor, std::string taskName)
		{
			this->profilerTaskColor = taskColor;
			this->profilerTaskName = taskName;
			return *this;
		}
		

		std::vector<BufferProxyId> inoutStorageBufferProxies;
		std::vector<ImageViewProxyId> inputImageViewProxies;
		std::vector<ImageViewProxyId> inoutStorageImageProxies;
		
		std::function<void(PassContext)> recordFunc;
		
		std::string profilerTaskName;
		uint32_t profilerTaskColor;
	};
	
	struct TransferPassDesc
	{
		TransferPassDesc()
		{
			profilerTaskName = "TransferPass";
			profilerTaskColor = legit::Colors::silver;
		}
		TransferPassDesc& SetSrcImages(std::vector<ImageViewProxyId>&& _srcImageViewProxies)
		{
			this->srcImageViewProxies = std::move(_srcImageViewProxies);
			return *this;
		}

		TransferPassDesc& SetDstImages(std::vector<ImageViewProxyId>&& _dstImageViewProxies)
		{
			this->dstImageViewProxies = std::move(_dstImageViewProxies);
			return *this;
		}

		TransferPassDesc& SetSrcBuffers(std::vector<BufferProxyId>&& _srcBufferProxies)
		{
			this->srcBufferProxies = std::move(_srcBufferProxies);
			return *this;
		}

		TransferPassDesc& SetDstBuffers(std::vector<BufferProxyId>&& _dstBufferProxies)
		{
			this->dstBufferProxies = std::move(_dstBufferProxies);
			return *this;
		}

		TransferPassDesc& SetRecordFunc(std::function<void(PassContext)> _recordFunc)
		{
			this->recordFunc = _recordFunc;
			return *this;
		}

		TransferPassDesc& SetProfilerInfo(uint32_t taskColor, std::string taskName)
		{
			this->profilerTaskColor = taskColor;
			this->profilerTaskName = taskName;
			return *this;
		}

		std::vector<BufferProxyId> srcBufferProxies;
		std::vector<ImageViewProxyId> srcImageViewProxies;

		std::vector<BufferProxyId> dstBufferProxies;
		std::vector<ImageViewProxyId> dstImageViewProxies;

		std::function<void(PassContext)> recordFunc;

		std::string profilerTaskName;
		uint32_t profilerTaskColor;
	};
	
	struct ImagePresentPassDesc
	{
		ImagePresentPassDesc &SetImage(ImageViewProxyId _presentImageViewProxyId)
		{
			this->presentImageViewProxyId = _presentImageViewProxyId;
			return *this;
		}
		ImageViewProxyId presentImageViewProxyId;
	};
	
	struct FrameSyncBeginPassDesc
	{
	};
	struct FrameSyncEndPassDesc
	{
	};
	
}
