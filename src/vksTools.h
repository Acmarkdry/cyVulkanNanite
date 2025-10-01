#pragma once
#include <string_view>
#include "NaniteMesh/NaniteMesh.h"


class PBRTexture;
class VulkanExampleBase;

namespace vks
{
	class ShaderName
	{
	public:
		static constexpr std::string_view skyboxVert = "skybox.vert.spv";
		static constexpr std::string_view skyboxFrag = "skybox.frag.spv";
		static constexpr std::string_view pbrtextureVert = "pbrtexture.vert.spv";
		static constexpr std::string_view pbrtextureFrag = "pbrtexture.frag.spv";
		static constexpr std::string_view pbrtextureGeom = "pbrtexture.geom.spv";
		static constexpr std::string_view hwrasterizeVert = "hwrasterize.vert.spv"; 
		static constexpr std::string_view hwrasterizeFrag = "hwrasterize.frag.spv";
		static constexpr std::string_view hwrasterizeGeom = "hwrasterize.geom.spv";
		static constexpr std::string_view debugQuadVert = "debugQuad.vert.spv";
		static constexpr std::string_view debugQuadFrag = "debugQuad.frag.spv";
		static constexpr std::string_view shadingVert = "shading.vert.spv";
		static constexpr std::string_view shadingFrag = "shading.frag.spv";
	};
	
	class vksTools
	{
	public:
		
		void static createStagingBuffer(VulkanExampleBase& variableLink, VkBufferUsageFlags sorceMemoryProperty, VkDeviceSize srcBufferSize, void* srcBufferData, VkBufferUsageFlags targetMemoryProperty, vkglTF::Model::Vertices& targetStaingBuffer, bool cmdRestart = true);

		void static createStagingBuffer(VulkanExampleBase& variableLink, VkBufferUsageFlags sorceMemoryProperty, VkDeviceSize srcBufferSize, void* srcBufferData, VkBufferUsageFlags targetMemoryProperty, vkglTF::Model::Indices& targetStaingBuffer, bool cmdRestart = true);

		void static createStagingBuffer(VulkanExampleBase& variableLink, VkBufferUsageFlags sorceMemoryProperty, VkDeviceSize srcBufferSize, void* srcBufferData, VkBufferUsageFlags targetMemoryProperty, Buffer& targetStaingBuffer, bool cmdRestart = true);

		void static setPbrDescriptor(PBRTexture& pbrTexture);

		VkImageSubresourceRange static genDepthSubresourceRange();

		void static generateBRDFLUT(PBRTexture& pbrTexture);
		void static generateIrradianceCube(PBRTexture& pbrTexture);
		void static generatePrefilteredCube(PBRTexture& pbrTexture);
	};
}
