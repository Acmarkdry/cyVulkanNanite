#include "VulkanDescriptorManager.h"

void VulkanDescriptorManager::destroy()
{
	auto instance = VulkanDescriptorManager::getManager();
	
	if (instance != nullptr)
	{
		for (auto &[_, layout]: instance->descriptorSetLayouts)
		{
			vkDestroyDescriptorSetLayout(instance->device, layout, nullptr);
		}
		vkDestroyDescriptorPool(instance->device, instance->descriptorPool, nullptr);
	}
}

void VulkanDescriptorManager::addSetLayout(const std::string& setName, const std::vector<VkDescriptorSetLayoutBinding>& setBindings, uint32_t numSets)
{
	auto newLayout = std::make_pair(setBindings, numSets);
	if (numSets < 1)
	{
		throw std::runtime_error("VulkanDescriptorManager::addSetLayout: invalid number of descriptor sets");
	}
	if (descriptorSetLayoutBindings.contains(setName))
	{
		throw std::runtime_error("VulkanDescriptorManager::addSetLayout: two same name descriptor set");
	}
	descriptorSetLayoutBindings[setName] = newLayout;
}

void VulkanDescriptorManager::createLayoutsAndSets(VkDevice& device)
{
	this->device = device;
	if(descriptorSetLayouts.size() == descriptorSetLayoutBindings.size())
		return;
	
	std::vector<VkDescriptorPoolSize> poolSizes;
	std::unordered_map<VkDescriptorType,int> typeCount;
	uint32_t maxSets=0;

	for(const auto& [name, bindings]:descriptorSetLayoutBindings)
	{
		int numSets = bindings.second;
		maxSets += numSets;
		for(const auto& binding:bindings.first)
		{
			typeCount[binding.descriptorType] += numSets;
		}
	}
	
	for(const auto& [type, count]:typeCount)
	{
		poolSizes.emplace_back(vks::initializers::descriptorPoolSize(type, count));
	}

	VkDescriptorPoolCreateInfo descriptorPoolInfo =	vks::initializers::descriptorPoolCreateInfo(poolSizes, maxSets);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

	for(const auto& [name, bindings]:descriptorSetLayoutBindings)
	{
		int numSets = bindings.second;
		VkDescriptorSetLayoutCreateInfo layoutCI = vks::initializers::descriptorSetLayoutCreateInfo(bindings.first);
		VkDescriptorSetLayout setLayout;
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &setLayout));
		descriptorSetLayouts[name]=setLayout;
		for(int i=0;i<numSets;i++)
		{
			VkDescriptorSet set;
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &setLayout, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &set));
			descriptorSets[name].emplace_back(set);
		}
	}
}

void VulkanDescriptorManager::writeToSet(const std::string& layoutName, uint32_t set, uint32_t binding,
	VkDescriptorBufferInfo* buffer)
{
	auto key = descriptorSetLayoutBindings[layoutName].first[binding].descriptorType;
	auto writeSet = vks::initializers::writeDescriptorSet(descriptorSets[layoutName][set], key, binding, buffer);
	vkUpdateDescriptorSets(device, 1, &writeSet, 0, nullptr);
}

void VulkanDescriptorManager::writeToSet(const std::string& layoutName, uint32_t set, uint32_t binding,
	VkDescriptorImageInfo* image)
{
	auto key = descriptorSetLayoutBindings[layoutName].first[binding].descriptorType;
	auto writeSet = vks::initializers::writeDescriptorSet(descriptorSets[layoutName][set], key, binding, image);
	vkUpdateDescriptorSets(device, 1, &writeSet, 0, nullptr);
}

const VkDescriptorSet& VulkanDescriptorManager::getSet(const std::string& layoutName, uint32_t set)
{
	if (descriptorSets.contains(layoutName))
	{
		return descriptorSets[layoutName][set];
	}
	else
	{
		throw std::runtime_error("VulkanDescriptorManager::getSet: invalid descriptor set name");
	}
}

const VkDescriptorSetLayout& VulkanDescriptorManager::getSetLayout(const std::string& layoutName)
{
	if (descriptorSetLayouts.contains(layoutName))
	{
		return descriptorSetLayouts[layoutName];
	}
	else
	{
		throw std::runtime_error("VulkanDescriptorManager::getSetLayout: invalid descriptor set layout name");
	}
}
