﻿// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk_types.h"
#include "vk_frame.h"
#include "vk_mesh.h"
#include "vk_descriptors.h"
#include "player_camera.h"
#include "VkBootstrap.h"
#include "vk_buffers.h"

#include "input_manager.h"

constexpr uint32_t FRAME_OVERLAP = 5; //Number of frames that the CPU works on in tandem with the GPU

class VulkanEngine {
public:

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();
	void cleanup_swapchain();

	//run main loop
	void run();

	bool _isInitialized{ false };

	bool _gameLoopInterrupted{ false };

	VkExtent2D _windowExtent;

	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _chosenGPU;
	VkDevice _device;
	VkSurfaceKHR _surface;

	VkPhysicalDeviceProperties _gpuProperties;

	//Swapchain

	vkb::Swapchain _vkbSwapchain;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;

	//Commands

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	//Frames

	std::vector<VkFramebuffer> _framebuffers;

	uint64_t _frameNumber{ 0 };
	FrameData _frames[FRAME_OVERLAP];
	FrameData& get_current_frame();

	//Renderpass

	VkRenderPass _renderPass;

	struct SDL_Window* _window{ nullptr };

	//Pipeline

	VkPipelineLayout _meshPipelineLayout;
	VkPipeline _meshPipeline;
	VkPipeline _texturedMeshPipeline;

	//Scene Management

	GPUSceneParameters _sceneParameters;
	
	vkutil::WriteBuffer _sceneBuffer;
	vkutil::WriteBuffer _cameraBuffer;

	std::vector<RenderObject> _renderables;
	std::unordered_map<std::string, std::shared_ptr<Material>> _materials;
	std::unordered_map<std::string, std::shared_ptr<Mesh>> _meshes;

	std::shared_ptr<Material> create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);
	std::shared_ptr<Material> get_material(const std::string& name);
	std::shared_ptr<Mesh> get_mesh(const std::string& name);

	//Descriptors

	DescriptorSetAllocator _descriptorAllocator;
	DescriptorSetLayoutCache _descriptorSetLayoutCache;

	//Depth buffer

	VkFormat _depthFormat;
	AllocatedImage _depthImage;
	VkImageView _depthImageView;

	//Buffers

	VkDescriptorSetLayout _singleTextureSetLayout;

	//GPU Memory

	UploadContext _meshUploadContext;

	/// <summary>
	/// Immediately runs a command on a command buffer that is locally allocated and cleaned up.
	/// </summary>
	void immediate_submit(std::function<void(VkCommandBuffer commandBuffer)>&& function);

	//Memory Allocation

	VmaAllocator _vmaAllocator;

	//Cleanup

	/// <summary>
	/// Cleanup methods run upon closing the program. Ends up destroying the VkInstance and VkDevice.
	/// </summary>
	DeletionStack _mainDeletionStack;

	/// <summary>
	/// Cleanup methods run when the swapchain is rebuilt.
	/// </summary>
	DeletionStack _swapChainDeletionStack;

	//Controls

	PlayerCamera _playerCamera;
	InputManager _inputManager;

	//Textures

	std::unordered_map<std::string, Texture> _loadedTextures;

	void load_images();

	void draw();

	void init_vulkan();

	void init_swapchain();
	void init_commands();
	void init_sync_structures();
	void init_default_renderpass(const vkb::Swapchain& swapchain);

	void init_imgui();

	void init_framebuffers();
	void init_descriptors();
	void init_pipelines();
	void init_scene();
	void init_input();

	void recreate_swap_chain();

	void load_meshes();
	void upload_mesh(Mesh& pMesh);

	VkResult load_shader_module(const std::string filePath, VkShaderModule *outShaderModule);

	void draw_objects(VkCommandBuffer commandBuffer, const std::vector<RenderObject>& renderables);
};

class fastrng {
public:
	fastrng(uint64_t seed) : seed(seed) {};

	inline double get_double() {
		return double(get_uint64()) / 0x7FFF;
	}

	inline uint64_t get_uint64() {
		seed = (214013 * seed + 2531011);
		return (seed >> 16) & 0x7FFF;
	}

private:
	uint64_t seed;
};