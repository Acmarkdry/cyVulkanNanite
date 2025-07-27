#pragma once
#include "Pipeline.h"
#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

class VulkanDescriptorManager;

class PBRTexture: public VulkanExampleBase
{
	public:
		bool displaySkybox = true;

	struct Textures {
		vks::TextureCubeMap environmentCube;
		// Generated at runtime
		vks::Texture2D lutBrdf;
		vks::TextureCubeMap irradianceCube;
		vks::TextureCubeMap prefilteredCube;
		// Object texture maps
		vks::Texture2D albedoMap;
		vks::Texture2D normalMap;
		vks::Texture2D aoMap;
		vks::Texture2D metallicMap;
		vks::Texture2D roughnessMap;
		vks::Texture2D hizBuffer;
	} textures{};

	struct Meshes {
		vkglTF::Model skybox;
		vkglTF::Model object;
	} models;

	struct UniformBuffers {
		vks::Buffer scene;
		vks::Buffer skybox;
		vks::Buffer params;
	} uniformBuffers;

	struct UniformDataMatrices {
		glm::mat4 projection;
		glm::mat4 model;
		glm::mat4 view;
		glm::vec3 camPos;
	} uniformDataMatrices;

	struct UniformDataParams {
		glm::vec4 lights[4];
		float exposure = 4.5f;
		float gamma = 2.2f;
	} uniformDataParams;

	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	struct {
		VkPipeline skybox{ VK_NULL_HANDLE };
		VkPipeline pbr{ VK_NULL_HANDLE };
	} pipelines;
	

	PBRTexture() : VulkanExampleBase(true)
	{
		title = "Textured PBR with IBL";
		camera.type = Camera::CameraType::firstperson;
		camera.movementSpeed = 4.0f;
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
		camera.rotationSpeed = 0.25f;
		camera.setRotation({ -7.75f, 150.25f, 0.0f });
		camera.setPosition({ 0.7f, 0.1f, 1.7f });
	}

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
