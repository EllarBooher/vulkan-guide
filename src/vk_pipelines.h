#pragma once

#include "vk_shaders.h"

class PipelineBuilder {
public:
	std::vector<VkPipelineShaderStageCreateInfo> _shaderStages = {};
	VkPipelineVertexInputStateCreateInfo _vertexInputState = {};
	VkPipelineInputAssemblyStateCreateInfo _inputAssembly = {};
	VkViewport _viewport = {};
	VkRect2D _scissor = {};
	VkPipelineRasterizationStateCreateInfo _rasterizer = {};
	VkPipelineColorBlendAttachmentState _colorBlendAttachment = {};
	VkPipelineDepthStencilStateCreateInfo _depthStencil = {};

	VkPipelineMultisampleStateCreateInfo _multisampling = {};
	VkPipelineLayout _pipelineLayout = {};

	VkResult build_pipeline(VkDevice device, VkRenderPass pass, VkPipeline* pipeline);
	void set_shaders(ShaderEffect& effect);
};