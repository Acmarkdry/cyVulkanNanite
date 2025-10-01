#pragma once
#include "PBRTextureBuffer.h"
#include "Pipeline.h"
#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#include "../src/NaniteMesh/NaniteScene.h"
#include "../src/NaniteMesh/Const.h"

class VulkanDescriptorManager;

class PBRTexture : public VulkanExampleBase
{
public:
	PBRTexture();
	~PBRTexture() override;

	// 禁用拷贝
	PBRTexture(const PBRTexture&) = delete;
	PBRTexture& operator=(const PBRTexture&) = delete;

	// 虚函数覆盖
	void getEnabledFeatures() override;
	void prepare() override;
	void buildCommandBuffers() override;
	void render() override;
	void viewChanged() override;
	void OnUpdateUIOverlay(vks::UIOverlay* overlay) override;
	void setupDepthStencil() override;

	// 资源加载与初始化
	void loadAssets();
	void setupDescriptors();
	void preparePipelines();

	// IBL生成
	void generateBRDFLUT();
	void generateIrradianceCube();
	void generatePrefilteredCube();

	// Uniform缓冲区
	void prepareUniformBuffers();
	void updateUniformBuffers();
	void updateParams();

	// 缓冲区创建
	void createHizBuffer();
	void createCullingBuffers();
	void createErrorProjectionBuffers();
	void createNaniteScene();
	void createRasterizeBuffer();
	void createModelMatsBuffer();
	void createHWRasterizeFrameBuffer();

	void initLogSystem();

private:
	// 命令缓冲区辅助方法
	void recordComputeCommands(VkCommandBuffer cmdBuffer, size_t frameIndex);
	void recordRenderPassCommands(VkCommandBuffer cmdBuffer, const VkRenderPassBeginInfo& rpBeginInfo);
	void recordDepthCopyCommands(VkCommandBuffer cmdBuffer);
	void recordHizGenerationCommands(VkCommandBuffer cmdBuffer);
	void recordDebugQuadCommands(VkCommandBuffer cmdBuffer, const VkRenderPassBeginInfo& rpBeginInfo, const VkViewport& viewport, const VkRect2D& scissor);

	// 内存屏障辅助方法
	[[nodiscard]] static VkBufferMemoryBarrier createBufferBarrier(VkBuffer buffer, VkAccessFlags srcAccess, VkAccessFlags dstAccess);
	[[nodiscard]] static VkImageMemoryBarrier createImageBarrier(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess, const VkImageSubresourceRange& subresourceRange);

	// Pipeline创建辅助
	void createGraphicsPipelines();
	void createComputePipelines();

public:
	// 显示设置
	bool displaySkybox = true;

	// 资源
	vks::Textures textures;
	vks::Meshes models;
	vks::UniformBuffers uniformBuffers;
	vks::UniformDataMatrices uniformDataMatrices;
	vks::UniformDataParams uniformDataParams;

	// Pipeline
	VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
	vks::Pipelines pipelines;
	Pipeline hizComputePipeline;
	Pipeline depthCopyPipeline;
	Pipeline debugQuadPipeline;
	Pipeline cullingPipeline;
	Pipeline errorProjPipeline;
	
	Pipeline hwrastPipeline;
	Pipeline swComputePipeline;
	Pipeline mergeRastPipeline;
	Pipeline clearImagePipeline;
	Pipeline shadingPipeline;

	// HIZ相关
	std::vector<VkImageView> hizImageViews;
	VkSampler depthStencilSampler{VK_NULL_HANDLE};

	// Nanite相关
	Nanite::NaniteMesh naniteMesh;
	Nanite::NaniteScene scene;
	std::vector<glm::mat4> modelMats;
	std::vector<Nanite::ClusterInfo> clusterInfos;
	std::vector<Nanite::ErrorInfo> errorInfos;

	// Culling缓冲区
	vks::Buffer hwRIndicesBuffer;
	vks::Buffer hwRIDBuffer;
	vks::Buffer swRIndicesBuffer;
	vks::Buffer swRIDBuffer;
	vks::Buffer culledIndicesBuffer;
	vks::Buffer clustersInfoBuffer;
	vks::Buffer cullingUniformBuffer;
	vks::Buffer drawIndexedIndirectBuffer;
	vks::Buffer swIndirectDispatchBuffer;
	vks::Buffer swNumVerticesBuffer;
	
	struct SWRIndirectBuffer {
		uint32_t x = 0;
		uint32_t y = 0;
		uint32_t z = 0;
	}swrIndirectBuffer;
	
	vks::DrawIndexedIndirect drawIndexedIndirect{};

	// Error Projection缓冲区
	vks::Buffer errorInfoBuffer;
	vks::Buffer projectedErrorBuffer;
	vks::Buffer errorUniformBuffer;

	// Uniform数据
	vks::UBOCullingMatrices uboCullingMatrices;
	vks::UBOErrorMatrices uboErrorMatrices;
	
	vks::UBOShading uboShading;

	// Push常量
	struct CullingPushConstants
	{
		int numClusters;
		float threshold;
		alignas(4) bool useFrustrumOcclusionCulling = true;
		alignas(4) bool useSoftwareRasterization = true;
	} cullingPushConstants{};

	struct ErrorPushConstants
	{
		alignas(4) int numClusters;
		alignas(8) glm::vec2 screenSize;
	} errorPushConstants{};
	
	int thresholdInt = 500;
	double thresholdIntDiv = 1e6;
	int visClustersLevel = 0;
	struct RenderingPushConstants
	{
		int visClusters = 0;
	} renderPushConstants;
	
	// 光栅化 为了存储rasterize，需要额外进行一个render pass
	vks::RasterizeBuffer HWRasterizeBuffer, HWRVisBuffer, SWRasterizeBuffer, finalZBuffer, finalVisBuffer;
	vks::Buffer modelMatsBuffer;
	VkFramebuffer HWRasterizeFrameBuffer;
	VkRenderPass hwRasterizeRenderPass;
	
	void setupRenderPass() override;

private:
	// 常量
	static constexpr int WORKGROUP_SIZE_X = 8;
	static constexpr int WORKGROUP_SIZE_Y = 8;
	static constexpr int DISPATCH_GROUP_SIZE = 64;
	static constexpr bool ENABLE_DEBUG_QUAD = false;
};
