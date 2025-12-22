#pragma once
#include <unordered_map>

#include "InstanceBase.h"
#include "VulkanDevice.h"

enum class DescriptorType 
{
	Scene,
	hiz,
	depthCopy,
	debugQuad,
};

class VulkanDescriptorManager: public Singleton<VulkanDescriptorManager>
{
	
public:
	static void destroy();

	void addSetLayout(const DescriptorType layoutName, const std::vector<VkDescriptorSetLayoutBinding>& setBindings, uint32_t numSets = 1);
	void createLayoutsAndSets(VkDevice& device);
	// TODO 尝试使用模板编程
	void writeToSet(const DescriptorType layoutName, uint32_t set, uint32_t binding, VkDescriptorBufferInfo* buffer);
	void writeToSet(const DescriptorType layoutName, uint32_t set, uint32_t binding, VkDescriptorImageInfo* image);

	const VkDescriptorSet& getSet(const DescriptorType layoutName, uint32_t set);
	const VkDescriptorSetLayout& getSetLayout(const DescriptorType layoutName);

private:
	VkDevice device = nullptr;
	VkDescriptorPool descriptorPool = nullptr;
	std::unordered_map<DescriptorType, std::vector<VkDescriptorSet>> descriptorSets;
	std::unordered_map<DescriptorType, VkDescriptorSetLayout> descriptorSetLayouts;
	std::unordered_map<DescriptorType, std::pair<std::vector<VkDescriptorSetLayoutBinding>, uint32_t>> descriptorSetLayoutBindings;
};

