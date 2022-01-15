#include "vk_descriptors.h"
  
#include "logger.h"
#include <algorithm>

void DescriptorSetAllocator::init(VkDevice device)
{
	_poolSizes = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f }
	};

	_device = device;
}

void DescriptorSetAllocator::cleanup()
{
	for (auto pool : _freePools)
	{
		vkDestroyDescriptorPool(_device, pool, nullptr);
	}
	for (auto pool : _usedPools)
	{
		vkDestroyDescriptorPool(_device, pool, nullptr);
	}
}

void DescriptorSetAllocator::reset_pools()
{
	for (auto pool : _usedPools) {
		vkResetDescriptorPool(_device, pool, 0);
		_freePools.push_back(pool);
	}

	_usedPools.clear();
	_currentPool = VK_NULL_HANDLE;
}

bool DescriptorSetAllocator::allocate(VkDescriptorSet* outSet, VkDescriptorSetLayout layout)
{
	if (_currentPool == VK_NULL_HANDLE)
	{
		_currentPool = create_or_grab_pool();
		_usedPools.push_back(_currentPool);
	}

	VkDescriptorSetAllocateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	info.pNext = nullptr;

	info.descriptorPool = _currentPool;
	info.descriptorSetCount = 1;
	info.pSetLayouts = &layout;

	VkResult allocationResult = vkAllocateDescriptorSets(_device, &info, outSet);

	bool needReallocate = false;

	switch (allocationResult) {
	case VK_SUCCESS:
		return true;
	case VK_ERROR_FRAGMENTED_POOL:
	case VK_ERROR_OUT_OF_POOL_MEMORY:
		break;
	default:
		//Exceptional, most likely unrecoverable error
		LOG_ERROR("Failed descriptor set allocation with an unexpected result.")
		LOG_VKRESULT(allocationResult);
		return false;
	}

	//reallocate
	_currentPool = create_or_grab_pool();
	_usedPools.push_back(_currentPool);

	info.descriptorPool = _currentPool;

	allocationResult = vkAllocateDescriptorSets(_device, &info, outSet);

	if (allocationResult == VK_SUCCESS) {
		return true;
	}

	LOG_ERROR("Failed descriptor set allocation twice in a row.");
	LOG_VKRESULT(allocationResult);
	return false;
}

VkDescriptorPool DescriptorSetAllocator::create_pool(VkDevice device, DescriptorPoolSizes sizeWeights, uint32_t count, VkDescriptorPoolCreateFlags flags)
{
	std::vector<VkDescriptorPoolSize> sizes;
	sizes.reserve(sizeWeights.size());
	for (auto size : sizeWeights)
	{
		VkDescriptorType type = size.first;
		uint32_t maxNumberOfDescriptors = size.second * count;

		sizes.push_back({ type, maxNumberOfDescriptors });
	}

	VkDescriptorPoolCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.pNext = nullptr;

	info.flags = flags;
	info.maxSets = count;
	info.poolSizeCount = (uint32_t)sizes.size();
	info.pPoolSizes = sizes.data();

	VkDescriptorPool descriptorPool;
	VK_CHECK(vkCreateDescriptorPool(device, &info, nullptr, &descriptorPool));

	return descriptorPool;
}

VkDescriptorPool DescriptorSetAllocator::create_or_grab_pool()
{
	if (_freePools.size() > 0)
	{
		auto pool = _freePools.back();
		_freePools.pop_back();
		return pool;
	}
	else
	{
		return create_pool(_device, _poolSizes, 1000, 0);
	}
}

void DescriptorSetLayoutCache::init(VkDevice device)
{
	_device = device;
}

void DescriptorSetLayoutCache::cleanup()
{
	for (auto pair : _layoutCache)
	{
		vkDestroyDescriptorSetLayout(_device, pair.second, nullptr);
	}
}

VkDescriptorSetLayout DescriptorSetLayoutCache::create_layout(const VkDescriptorSetLayoutCreateInfo& info)
{
	DescriptorSetLayoutInfo ourInfo{};
	ourInfo._bindings.reserve(info.bindingCount);
	bool isSorted = true;
	int lastBinding = -1;

	//We copy the vulkan layout create info into a custom format that we can hash & sort & compare
	for (int i = 0; i < info.bindingCount; i++)
	{
		DescriptorSetLayoutBinding ourBinding{};
		VkDescriptorSetLayoutBinding vkBinding = info.pBindings[i];

		ourBinding._binding = vkBinding.binding;
		ourBinding._descriptorType = vkBinding.descriptorType;
		ourBinding._descriptorCount = vkBinding.descriptorCount;
		ourBinding._stageFlags = vkBinding.stageFlags;

		ourInfo._bindings.push_back(ourBinding);

		if (vkBinding.binding > lastBinding)
		{
			lastBinding = vkBinding.binding;
		}
		else
		{
			isSorted = false;
		}
	}

	if (!isSorted)
	{
		std::sort(ourInfo._bindings.begin(), ourInfo._bindings.end(), [](DescriptorSetLayoutBinding& a, DescriptorSetLayoutBinding& b) {
			return a._binding < b._binding;
			});
	}

	auto cachedLayout = _layoutCache.find(ourInfo);
	if (cachedLayout != _layoutCache.end()) 
	{
		return (*cachedLayout).second;
	}
	else 
	{
		VkDescriptorSetLayout layout;
		VK_CHECK(vkCreateDescriptorSetLayout(_device, &info, nullptr, &layout));

		_layoutCache[ourInfo] = layout;
		return layout;
	}
}

bool DescriptorSetLayoutCache::DescriptorSetLayoutInfo::operator==(const DescriptorSetLayoutInfo& other) const
{
	return other._flags == _flags && other._bindings == _bindings;
}

size_t DescriptorSetLayoutCache::DescriptorSetLayoutInfo::hash() const
{
	size_t result = std::hash<size_t>()(_bindings.size() | _flags << 32);

	for (auto& binding : _bindings)
	{
		result ^= binding.hash();
	}

	return result;
}

bool DescriptorSetLayoutCache::DescriptorSetLayoutBinding::operator==(const DescriptorSetLayoutBinding& other) const
{
	return other._binding == _binding
		&& other._descriptorType == _descriptorType
		&& other._descriptorCount == _descriptorCount
		&& other._stageFlags == _stageFlags;
}

size_t DescriptorSetLayoutCache::DescriptorSetLayoutBinding::hash() const
{
	size_t raw = _binding | _descriptorType << 8 | _descriptorCount << 16 | _stageFlags << 24;

	return std::hash<size_t>()(raw);
}

DescriptorSetBuilder DescriptorSetBuilder::begin(DescriptorSetLayoutCache& layoutCache, DescriptorSetAllocator& allocator)
{
	DescriptorSetBuilder builder;

	builder._layoutCache = &layoutCache;
	builder._allocator = &allocator;

	return builder;
}

DescriptorSetBuilder& DescriptorSetBuilder::bind_buffer(uint32_t binding, VkDescriptorBufferInfo& bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
{
	VkDescriptorSetLayoutBinding newBinding{};

	newBinding.descriptorCount = 1;
	newBinding.descriptorType = type;
	newBinding.pImmutableSamplers = nullptr;
	newBinding.stageFlags = stageFlags;
	newBinding.binding = binding;

	_bindings.push_back(newBinding);

	VkWriteDescriptorSet newWrite{};
	newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	newWrite.pNext = nullptr;

	newWrite.descriptorCount = 1;
	newWrite.descriptorType = type;
	newWrite.pBufferInfo = &bufferInfo;
	newWrite.dstBinding = binding;

	_writes.push_back(newWrite);
	return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::bind_image(uint32_t binding, VkDescriptorImageInfo& imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
{
	VkDescriptorSetLayoutBinding newBinding{};

	newBinding.descriptorCount = 1;
	newBinding.descriptorType = type;
	newBinding.pImmutableSamplers = nullptr;
	newBinding.stageFlags = stageFlags;
	newBinding.binding = binding;

	_bindings.push_back(newBinding);

	VkWriteDescriptorSet newWrite{};
	newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	newWrite.pNext = nullptr;

	newWrite.descriptorCount = 1;
	newWrite.descriptorType = type;
	newWrite.pImageInfo = &imageInfo;
	newWrite.dstBinding = binding;
	//we delay setting the destination set until building
	newWrite.dstSet = 0;

	_writes.push_back(newWrite);
	return *this;
}

bool DescriptorSetBuilder::build(VkDescriptorSet& outSet, VkDescriptorSetLayout& outLayout)
{
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = nullptr;

	layoutInfo.pBindings = _bindings.data();
	layoutInfo.bindingCount = _bindings.size();
	layoutInfo.flags = 0;

	outLayout = _layoutCache->create_layout(layoutInfo);

	bool success = _allocator->allocate(&outSet, outLayout);
	if (!success) { return false; }

	for (VkWriteDescriptorSet& write : _writes)
	{
		write.dstSet = outSet;
	}

	vkUpdateDescriptorSets(_allocator->_device, _writes.size(), _writes.data(), 0, nullptr);

	return true;
}

bool DescriptorSetBuilder::build(VkDescriptorSet& outSet)
{
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = nullptr;

	layoutInfo.pBindings = _bindings.data();
	layoutInfo.bindingCount = _bindings.size();
	layoutInfo.flags = 0;

	auto layout = _layoutCache->create_layout(layoutInfo);

	bool success = _allocator->allocate(&outSet, layout);
	if (!success) { return false; }

	for (VkWriteDescriptorSet& write : _writes)
	{
		write.dstSet = outSet;
	}

	vkUpdateDescriptorSets(_allocator->_device, _writes.size(), _writes.data(), 0, nullptr);

	return true;
}
