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
	bool displaySkybox = true;

	vks::Textures textures;
	vks::Meshes models;
	vks::UniformBuffers uniformBuffers;
	vks::UniformDataMatrices uniformDataMatrices;
	vks::UniformDataParams uniformDataParams;
	VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
	vks::Pipelines pipelines;
	vks::UBOCullingMatrices uboCullingMatrices;
	vks::UBOErrorMatrices uboErrorMatrices;

	PBRTexture();

	~PBRTexture() override;

	void getEnabledFeatures() override;
	void loadAssets();
	void setupDescriptors();
	void preparePipelines();
	// Generate a BRDF integration map used as a look-up-table (stores roughness / NdotV)
	void generateBRDFLUT();
	// Generate an irradiance cube map from the environment cube map
	void generateIrradianceCube();
	// Prefilter environment cubemap
	// See https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/
	void generatePrefilteredCube();
	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers();
	void updateUniformBuffers();
	void updateParams();
	void prepare() override;
	void buildCommandBuffers() override;
	void render() override;
	void viewChanged() override;
	void OnUpdateUIOverlay(vks::UIOverlay* overlay) override;

	void createHizBuffer();
	void setupDepthStencil() override;
	void createCullingBuffers();
	void createErrorProjectionBuffers();

	void initLogSystem();

	/*hiz buffer相关的类*/
	std::vector<VkImageView> hizImageViews;
	Pipeline hizComputePipeline;
	Pipeline depthCopyPipeline;
	Pipeline debugQuadPipeline;

	VkSampler depthStencilSampler;

	// nanite mesh相关
	Nanite::NaniteMesh naniteMesh;
	Nanite::NaniteScene scene;
	
	void createNaniteScene();
	
	glm::mat4 model0 = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 3)), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 model1 = glm::scale(glm::mat4(1.0f), glm::vec3(0.2f, 0.2f, 0.03f));
	std::vector<glm::mat4> modelMats = {
		glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 3)), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		glm::mat4(1.0f),
		glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.0f)),
		glm::mat4(1.0f),
		glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.0f)),
	};
	
	std::vector<Nanite::ClusterInfo> clusterInfos;
	std::vector<Nanite::ErrorInfo> errorInfos;

	vks::Buffer culledIndicesBuffer;
	vks::Buffer clustersInfoBuffer;
	vks::Buffer cullingUniformBuffer;
	vks::Buffer drawIndexedIndirectBuffer;
	vks::DrawIndexedIndirect drawIndexedIndirect;

	vks::Buffer errorInfoBuffer;
	vks::Buffer projectedErrorBuffer;
	vks::Buffer errorUniformBuffer;
	
	struct CullingPushConstants
	{
		int numClusters;
	} cullingPushConstants;
	struct ErrorPushConstants
	{
		alignas(4) int numClusters;
		alignas(8) glm::vec2 screenSize;
	} errorPushConstants;
	Pipeline cullingPipeline;
	Pipeline errorProjPipeline;
};
