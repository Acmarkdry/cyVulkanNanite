#pragma once
#include <unordered_map>

#include "InstanceBase.h"
#include "VulkanDevice.h"

class VulkanDescriptorManager: public Singleton<VulkanDescriptorManager>
{
	
public:
	static void destroy();

	void addSetLayout(const std::string& layoutName, const std::vector<VkDescriptorSetLayoutBinding>& setBindings, uint32_t numSets = 1);
	void createLayoutsAndSets(VkDevice& device);
	// TODO 尝试使用模板编程
	void writeToSet(const std::string& layoutName, uint32_t set, uint32_t binding, VkDescriptorBufferInfo* buffer);
	void writeToSet(const std::string& layoutName, uint32_t set, uint32_t binding, VkDescriptorImageInfo* image);

	const VkDescriptorSet& getSet(const std::string& layoutName, uint32_t set);
	const VkDescriptorSetLayout& getSetLayout(const std::string& layoutName);

private:
	VkDevice device = nullptr;
	VkDescriptorPool descriptorPool = nullptr;
	std::unordered_map<std::string, std::vector<VkDescriptorSet>> descriptorSets;
	std::unordered_map<std::string, VkDescriptorSetLayout> descriptorSetLayouts;
	std::unordered_map<std::string, std::pair<std::vector<VkDescriptorSetLayoutBinding>, uint32_t>> descriptorSetLayoutBindings;
};

