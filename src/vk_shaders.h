#pragma once

#include <vulkan/vulkan.h>

#include <vk_types.h>
#include <vector>
#include <array>
#include <string_view>
#include "vk_descriptors.h"

struct ShaderModuleDescriptorSetLayout {
	ShaderModuleDescriptorSetLayout(uint32_t set, uint32_t binding, VkDescriptorType type, uint32_t descriptorCount) : _set(set), _binding(binding), _descriptorType(type), _descriptorCount(descriptorCount) {}

	uint32_t _set;
	uint32_t _binding;
	VkDescriptorType _descriptorType;
	VkShaderStageFlags _stageFlags = 0; //ignored when submitting to shadereffect
	uint32_t _descriptorCount;
};

struct ShaderModule {
	VkShaderModule _vkShaderModule;
	std::vector<ShaderModuleDescriptorSetLayout> _requiredBindings;

	static VkResult load(VkDevice device, const std::string_view& filepath, std::vector<ShaderModuleDescriptorSetLayout> descriptors, ShaderModule* outShaderModule);
};

// Represents a group of shaders that we might compose a pipeline of. The end goal of this is the pipeline layout, see vk_descriptors.h for the structures for writing to descriptor sets at runtime.
struct ShaderEffect {
	void add_stage(ShaderModule shaderModule, VkShaderStageFlagBits stage);
	void fill_stages(std::vector<VkPipelineShaderStageCreateInfo>& pipelineStages);
	VkResult build_layout(VkDevice device, DescriptorSetLayoutCache& layoutCache);

	VkPipelineLayout _builtLayout;
private:
	void add_required_binding(ShaderModuleDescriptorSetLayout binding);

	std::vector<ShaderModuleDescriptorSetLayout> _requiredBindings;

	struct ShaderStage {
		ShaderModule _shaderModule;
		VkShaderStageFlagBits _stage;
	};

	std::vector<ShaderStage> _stages;
};