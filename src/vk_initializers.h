#pragma once

#include <vk_types.h>

namespace vkinit {
	VkCommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags);
	VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count, VkCommandBufferLevel level);
	VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags);

	VkSubmitInfo submit_info(VkPipelineStageFlags waitStage, const std::vector<VkSemaphore>& waitSemaphores, const std::vector<VkSemaphore>& signalSemaphores, const std::vector<VkCommandBuffer>& commandBuffers);

	VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shaderModule);
	VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info();
	VkPipelineMultisampleStateCreateInfo multisampling_state_create_info();

	VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info(VkPrimitiveTopology topology);
	VkPipelineRasterizationStateCreateInfo rasterization_state_create_info(VkPolygonMode polygonMode);
	VkPipelineColorBlendAttachmentState color_blend_attachment_state();

	VkSamplerCreateInfo sampler_create_info(VkFilter filters, VkSamplerAddressMode samplerAddressMode);

	VkRenderPassBeginInfo renderpass_begin_info(VkRenderPass renderPass, VkExtent2D extent, VkFramebuffer framebuffer);

	VkPipelineLayoutCreateInfo pipeline_layout_create_info();

	VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info(bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp);

	VkImageCreateInfo image_create_info(VkFormat format, VkImageUsageFlags flags, VkExtent3D extent);
	VkImageViewCreateInfo imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectsFlags);

	VkDescriptorSetLayoutBinding descriptorset_layout_binding(VkDescriptorType type, VkShaderStageFlagBits stageFlags, uint32_t binding);

	VkWriteDescriptorSet write_descriptor_image(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo& imageInfo, uint32_t binding);
	VkWriteDescriptorSet write_descriptorset(VkDescriptorType type, VkDescriptorSet dstSet, const VkDescriptorBufferInfo& bufferInfo, uint32_t binding);
}

