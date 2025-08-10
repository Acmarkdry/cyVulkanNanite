#pragma once
#include <unordered_set>

#include "VulkanglTFModel.h"
#include "VulkanTexture.h"

namespace vks
{
	struct Textures
	{
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
		
		void destroy();
	};
	
	struct Meshes
	{
		vkglTF::Model skybox;
		vkglTF::Model object;
	};
	
	struct UniformBuffers {
		Buffer scene;
		Buffer skybox;
		Buffer params;
		
		void destroy();
	};
	
	struct UniformDataMatrices {
		glm::mat4 projection;
		glm::mat4 model;
		glm::mat4 view;
		glm::vec3 camPos;
	};
	
	struct UniformDataParams {
		glm::vec4 lights[4];
		float exposure = 4.5f;
		float gamma = 2.2f;
	};
	
	struct Pipelines
	{
		VkPipeline skybox{ VK_NULL_HANDLE };
		VkPipeline pbr{ VK_NULL_HANDLE };
	};
	
	struct DrawIndexedIndirect
	{
		uint32_t indexCount;
		uint32_t instanceCount;
		uint32_t firstIndex;
		uint32_t firstInstance;
		uint32_t vertexOffset;
	};
	
	struct UBOCullingMatrices
	{
		glm::mat4 model;
		glm::mat4 lastView;
		glm::mat4 lastProj;
	};

	struct UBOErrorMatrices
	{
		glm::mat4 view;
		glm::mat4 proj;
		alignas(16) glm::vec3 camUp;
		alignas(16) glm::vec3 camRight;
	};
	
	class VulkanResourceTracker
	{
	private:
		std::unordered_set<VkImageView> imageViewSet;
	
	public:		
		void createImageView(VkDevice& device, const VkImageViewCreateInfo& pCreateInfo, VkImageView& imageView);
		
		void destroyImageView(VkDevice& device, VkImageView& view);
		
		void checkLeaks();
	};
	
}
