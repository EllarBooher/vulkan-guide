#pragma once

#include <vulkan/vulkan.h>

#include "vk_buffers.h"

struct FrameData {
	VkSemaphore _presentSemaphore, _renderSemaphore;
	VkFence _renderFence;

	VkDescriptorSet _globalDescriptorSet;
	VkDescriptorSet _objectDescriptorSet;

	vkutil::WriteBuffer _objectBuffer;
	vkutil::WriteBuffer _indirectDrawCommands;

	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;
};