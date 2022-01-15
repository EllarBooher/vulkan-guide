#pragma once

#include <vulkan/vulkan.h>
#include <stack>
#include <vector>
#include <unordered_map>

typedef std::vector<std::pair<VkDescriptorType, float>> DescriptorPoolSizes;

//This is a pool of vkdescriptorpool, but also handles the allocation of the descriptorsets
class DescriptorSetAllocator {
public:
	void init(VkDevice device);
	void reset_pools();
	bool allocate(VkDescriptorSet* outSet, VkDescriptorSetLayout layout);
	void cleanup();
	VkDevice _device;

private:
	VkDescriptorPool create_pool(VkDevice device, DescriptorPoolSizes sizeWeights, uint32_t count, VkDescriptorPoolCreateFlags flags);
	VkDescriptorPool create_or_grab_pool();
	VkDescriptorPool _currentPool{ VK_NULL_HANDLE };
	DescriptorPoolSizes _poolSizes;
	std::vector<VkDescriptorPool> _usedPools;
	std::vector<VkDescriptorPool> _freePools;

	//TODO: diagnostics to record allocations to adjust pool weights
};

//Not global, so be careful where you instantiate a copy since they will not share caches, although all the returned layouts will still be correct.
class DescriptorSetLayoutCache {
public:
	void init(VkDevice device);
	void cleanup();
	VkDescriptorSetLayout create_layout(const VkDescriptorSetLayoutCreateInfo& info);
private:
	struct DescriptorSetLayoutBinding {
		uint32_t _binding;
		VkDescriptorType _descriptorType;
		uint32_t _descriptorCount;
		VkShaderStageFlags _stageFlags;

		bool operator==(const DescriptorSetLayoutBinding& other) const;
		size_t hash() const;
	};
	struct DescriptorSetLayoutInfo {
		VkDescriptorSetLayoutCreateFlags _flags;
		std::vector<DescriptorSetLayoutBinding> _bindings;

		bool operator==(const DescriptorSetLayoutInfo& other) const;
		size_t hash() const;
	};
	struct DescriptorSetLayoutHash {
		size_t operator()(const DescriptorSetLayoutInfo& k) const {
			return k.hash();
		}
	};
	std::unordered_map<DescriptorSetLayoutInfo, VkDescriptorSetLayout, DescriptorSetLayoutHash> _layoutCache;
	VkDevice _device;
};

class DescriptorSetBuilder {
public:
	static DescriptorSetBuilder begin(DescriptorSetLayoutCache& layoutCache, DescriptorSetAllocator& allocator);

	DescriptorSetBuilder& bind_buffer(uint32_t binding, VkDescriptorBufferInfo& bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);
	DescriptorSetBuilder& bind_image(uint32_t binding, VkDescriptorImageInfo& imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);

	bool build(VkDescriptorSet& outSet, VkDescriptorSetLayout& outLayout);
	bool build(VkDescriptorSet& outSet);

private:
	std::vector<VkWriteDescriptorSet> _writes;
	std::vector<VkDescriptorSetLayoutBinding> _bindings;

	DescriptorSetLayoutCache* _layoutCache;
	DescriptorSetAllocator* _allocator;
};