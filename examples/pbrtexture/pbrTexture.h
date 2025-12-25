#pragma once
#include "PBRTextureBuffer.h"
#include "Pipeline.h"
#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

class VulkanDescriptorManager;

class PBRTexture: public VulkanExampleBase
{
	public:
		bool displaySkybox = true;
	
		vks::Textures textures;
		vks::Meshes models;
		vks::UniformBuffers uniformBuffers;
		vks::UniformDataMatrices uniformDataMatrices;
		vks::UniformDataParams uniformDataParams;
		VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
		vks::Pipelines pipelines;
	
	PBRTexture();

	~PBRTexture();

	virtual void getEnabledFeatures() override;
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
	void prepare();
	void buildCommandBuffers();
	virtual void render();
	virtual void viewChanged() override;
	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay);

	void createHizBuffer();
	void setupDepthStencil() override;
	
	/*hiz buffer相关的类*/
	std::vector<VkImageView> hizImageViews;
	Pipeline hizComputePipeline;
	Pipeline depthCopyPipeline;
	Pipeline debugQuadPipeline;
	
	VkSampler depthStencilSampler;
};
