// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"
#include "glm/glm.hpp"

#include <vector>
#include <string>
#include <stack>
#include <functional>
#include <memory>

//we will add our main reusable types here

struct Mesh;

struct AllocatedBuffer {
	VkBuffer _buffer;
	VmaAllocation _allocation;
};

struct AllocatedImage {
	VkImage _image;
	VmaAllocation _allocation;
};

struct GPUCameraData {
	glm::mat4 viewproj;
};

struct GPUSceneParameters {
	glm::vec4 fogColor;
	glm::vec4 fogDistances;
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection;
	glm::vec4 sunlightColor;
};

struct GPUObjectData {
	glm::mat4 modelMatrix;
	glm::vec4 color;
};

struct FrameData {
	AllocatedBuffer cameraBuffer;
	VkDescriptorSet globalDescriptorSet;

	AllocatedBuffer objectBuffer;
	VkDescriptorSet objectDescriptorSet;

	VkSemaphore _presentSemaphore, _renderSemaphore;
	VkFence _renderFence;

	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;
};

struct DeletionStack {
	void push_function(std::function<void()>&& function) {
		deletors.push(function);
	}

	void flush() {
		while (!deletors.empty()) {
			(deletors.top())();
			deletors.pop();
		}
	}

private:
	std::stack<std::function<void()>> deletors;
};

struct UploadContext {
	VkFence _uploadFence;
	VkCommandPool _commandPool;
};

struct MeshPushConstants {
	glm::vec4 data;
	glm::mat4 render_matrix;
};

struct Texture {
	AllocatedImage image;
	VkImageView imageView;
};

struct Material {
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSet textureSet{ VK_NULL_HANDLE };
};

struct RenderObject {
	std::shared_ptr<Mesh> pMesh;
	std::shared_ptr<Material> pMaterial;

	glm::vec4 color;
	glm::mat4 transform;
};
