#include "vk_shaders.h"
#include "vk_initializers.h"
#include "logger.h"
#include "vk_descriptors.h"

#include <fstream>

void ShaderEffect::add_stage(ShaderModule shaderModule, VkShaderStageFlagBits stage)
{
	ShaderStage newStage = { shaderModule, stage };
	_stages.push_back(newStage);

	auto bindings = shaderModule._requiredBindings;

	for (auto& binding : bindings)
	{
		binding._stageFlags |= stage;

		add_required_binding(binding);
	}
}

void ShaderEffect::fill_stages(std::vector<VkPipelineShaderStageCreateInfo>& pipelineStagesToFill)
{
	for (auto& stage : _stages)
	{
		pipelineStagesToFill.push_back(vkinit::pipeline_shader_stage_create_info(stage._stage, stage._shaderModule._vkShaderModule));
	}
}

VkResult ShaderEffect::build_layout(VkDevice device, DescriptorSetLayoutCache& layoutCache)
{
	std::array<std::vector<VkDescriptorSetLayoutBinding>, 4> setBindings;

	for (auto& requiredBinding : _requiredBindings)
	{
		auto binding = VkDescriptorSetLayoutBinding{};
		binding.binding = requiredBinding._binding;
		binding.descriptorCount = requiredBinding._descriptorCount;
		binding.descriptorType = requiredBinding._descriptorType;
		binding.pImmutableSamplers = nullptr; //TODO: immutable sampler is being discarded here, fully implement it later
		binding.stageFlags = requiredBinding._stageFlags;

		setBindings[requiredBinding._set].push_back(binding);
	}

	std::vector<VkDescriptorSetLayout> setLayouts;
	for (int s = 0; s < 4; s++)
	{
		auto info = VkDescriptorSetLayoutCreateInfo{};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.pNext = nullptr;

		info.flags = 0;
		info.bindingCount = setBindings[s].size();
		info.pBindings = setBindings[s].data();

		auto layout = layoutCache.create_layout(info);

		setLayouts.push_back(layout);
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkinit::pipeline_layout_create_info();

	pipelineLayoutInfo.pSetLayouts = setLayouts.data();
	pipelineLayoutInfo.setLayoutCount = setLayouts.size();

	pipelineLayoutInfo.pushConstantRangeCount = 0; //TODO: implement push constants

	return vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &_builtLayout);
}

void ShaderEffect::add_required_binding(ShaderModuleDescriptorSetLayout binding)
{
	for (auto& existingBinding : _requiredBindings)
	{
		//Different shaders cannot have conflicting bindings (as far as I know TODO: research this)
		if (binding._set == existingBinding._set &&
			binding._binding == existingBinding._binding)
		{
			if (binding._descriptorType == existingBinding._descriptorType &&
				binding._descriptorCount == existingBinding._descriptorCount)
			{
				existingBinding._stageFlags |= binding._stageFlags;
			}
			else
			{
				LOG_FATAL("Shaders with incompatible bindings added to the same ShaderEffect.");
			}
			return;
		}
	}

	_requiredBindings.push_back(binding);
}

VkResult ShaderModule::load(VkDevice device, const std::string_view& filepath, std::vector<ShaderModuleDescriptorSetLayout> descriptors, ShaderModule* outShaderModule)
{
	std::ifstream file(filepath.data(), std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	size_t fileSize = (size_t)file.tellg();

	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	file.seekg(0);

	file.read((char*)buffer.data(), fileSize);

	file.close();

	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.pNext = nullptr;

	shaderModuleCreateInfo.codeSize = buffer.size() * sizeof(uint32_t);
	shaderModuleCreateInfo.pCode = buffer.data();

	outShaderModule->_requiredBindings = descriptors;

	return vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &outShaderModule->_vkShaderModule);
}
