#define VMA_IMPLEMENTATION

#include "vk_engine.h"
#include "vk_textures.h"
#include "vk_shaders.h"

#include "vk_buffers.h"
#include "vk_frame.h"

#include "logger.h"

#include <iostream>

#include <array>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <SDL_syswm.h>

#include <vk_types.h>
#include <vk_initializers.h>

#include <fstream>
#include <glm/gtx/transform.hpp>

#include "vk_pipelines.h"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"

//Bootstrap
#include "VkBootstrap.h"

int watch_for_window_move(void* userData, SDL_Event* event)
{
	bool* _gameLoopInterrupted = (bool*)userData;

	if (event->type == SDL_SYSWMEVENT)
	{
		SDL_SysWMmsg* sys_event = event->syswm.msg;
#if defined (WIN32)

		UINT message = sys_event->msg.win.msg;

		switch (message)
		{
		case WM_ENTERSIZEMOVE: //these events only exist on newer windows
		case WM_ENTERMENULOOP:
			*_gameLoopInterrupted = true;
			break;
		}
#endif
	}
	return 0;
}

void VulkanEngine::init()
{
	LOG_INFO("Engine Initialization");

	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	
	_windowExtent = { 1700, 900 };

	_window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_windowExtent.width,
		_windowExtent.height,
		windowFlags
	);
	SDL_AddEventWatch(watch_for_window_move, &_gameLoopInterrupted); //We do this since windows (or SDL) locks out the main loop while a window is being dragged, even if no events are being passed.
	SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE); //System specific to windows

	init_vulkan();
	init_swapchain();
	init_commands();
	init_default_renderpass(_vkbSwapchain);
	init_framebuffers();
	init_sync_structures();

	init_imgui();

	init_descriptors();
	init_pipelines();
	init_input();

	load_images();
	load_meshes();

	init_scene();

	//everything went fine
	_isInitialized = true;
}

void VulkanEngine::recreate_swap_chain()
{
	vkDeviceWaitIdle(_device);

	cleanup_swapchain();

	init_swapchain();
	init_default_renderpass(_vkbSwapchain);
	init_framebuffers();
	init_commands();
}

void VulkanEngine::init_vulkan()
{
	LOG_INFO("Vulkan Initialization");

	vkb::InstanceBuilder builder;
	auto instanceBuildResult = builder.set_app_name("Example Vulkan Application")
		.request_validation_layers(true)
		.require_api_version(1, 2, 0)
		.use_default_debug_messenger()
		.build();

	VKB_CHECK(instanceBuildResult);

	vkb::Instance vkbInstance = instanceBuildResult.value();

	_instance = vkbInstance.instance;
	_debug_messenger = vkbInstance.debug_messenger;

	if (SDL_Vulkan_CreateSurface(_window, _instance, &_surface) == SDL_TRUE)
	{
		LOG_SUCCESS("SDL Surface Initialized");
	}
	else
	{
		LOG_FATAL("SDL Surface failed to Initialize");
	}

	VkPhysicalDeviceFeatures feats{};
	feats.multiDrawIndirect = true;
	
	VkPhysicalDeviceVulkan11Features v11feats{};
	v11feats.shaderDrawParameters = true;

	vkb::PhysicalDeviceSelector selector{ vkbInstance };
	auto physicalDeviceSelectResult = selector
		.set_minimum_version(1, 2)
		.set_surface(_surface)
		.set_required_features(feats)
		.set_required_features_11(v11feats)
		.select();

	VKB_CHECK(physicalDeviceSelectResult);

	vkb::PhysicalDevice vkbPhysicalDevice = physicalDeviceSelectResult.value();

	vkb::DeviceBuilder deviceBuilder{ vkbPhysicalDevice };

	VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParameters{};
	shaderDrawParameters.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
	shaderDrawParameters.pNext = nullptr;
	
	shaderDrawParameters.shaderDrawParameters = true;
	
	deviceBuilder.add_pNext(&shaderDrawParameters);

	auto deviceBuildResult = deviceBuilder.build();

	VKB_CHECK(deviceBuildResult);

	vkb::Device vkbDevice = deviceBuildResult.value();

	_device = vkbDevice.device;
	_chosenGPU = vkbPhysicalDevice.physical_device;

	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = _chosenGPU;
	allocatorInfo.device = _device;
	allocatorInfo.instance = _instance;
	VK_CHECK(vmaCreateAllocator(&allocatorInfo, &_vmaAllocator));

	vkGetPhysicalDeviceProperties(_chosenGPU, &_gpuProperties);

	LOG_SUCCESS("Vulkan Initialization Complete");
}

void VulkanEngine::init_imgui()
{
	VkDescriptorPoolSize poolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 1000;
	poolInfo.poolSizeCount = std::size(poolSizes);
	poolInfo.pPoolSizes = poolSizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(_device, &poolInfo, nullptr, &imguiPool));

	//Imgui's initialization

	ImGui::CreateContext();

	ImGui_ImplSDL2_InitForVulkan(_window);

	ImGui_ImplVulkan_InitInfo initInfo{};
	initInfo.Instance = _instance;
	initInfo.PhysicalDevice = _chosenGPU;
	initInfo.Device = _device;
	initInfo.Queue = _graphicsQueue;
	initInfo.DescriptorPool = imguiPool;
	initInfo.MinImageCount = FRAME_OVERLAP;
	initInfo.ImageCount = FRAME_OVERLAP;
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&initInfo, _renderPass);

	immediate_submit([&](VkCommandBuffer cmd) {
		ImGui_ImplVulkan_CreateFontsTexture(cmd);
		});

	ImGui_ImplVulkan_DestroyFontUploadObjects();

	_mainDeletionStack.push_function([=]() {
		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
		});

}

void VulkanEngine::init_swapchain()
{
	LOG_INFO("Swapchain Initialization");

	VkExtent2D desiredExtent{};
	int32_t w;
	int32_t h;
	SDL_GetWindowSize(_window, &w, &h);
	desiredExtent.width = w;
	desiredExtent.height = h;

	vkb::SwapchainBuilder swapchainBuilder(_chosenGPU, _device, _surface);
	auto vkbSwapchainBuildResult = swapchainBuilder
		.use_default_format_selection()
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(desiredExtent.width, desiredExtent.height)
		.build();

	VKB_CHECK(vkbSwapchainBuildResult);

	vkb::Swapchain vkbSwapchain = vkbSwapchainBuildResult.value();

	_windowExtent = vkbSwapchain.extent; //Get the actual extent that the swapchain was created with (possibly smaller than desiredExtent)

	_vkbSwapchain = vkbSwapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();

	_swapChainDeletionStack.push_function([=]() {
		for (int i = 0; i < _swapchainImageViews.size(); ++i)
		{
			vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
		}
		//No need to destroy images explicitely, as they are presentable and are destroyed with the swapchain
		vkDestroySwapchainKHR(_device, _vkbSwapchain.swapchain, nullptr);
	});

	//Depth Image

	VkExtent3D depthImageExtent = {
		_windowExtent.width,
		_windowExtent.height,
		1
	};

	_depthFormat = VK_FORMAT_D32_SFLOAT;

	VkImageCreateInfo depthImageCreateInfo = vkinit::image_create_info(_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);

	VmaAllocationCreateInfo depthImageAllocationInfo = {};
	depthImageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	depthImageAllocationInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VK_CHECK(vmaCreateImage(_vmaAllocator, &depthImageCreateInfo, &depthImageAllocationInfo, &_depthImage._image, &_depthImage._allocation, nullptr));

	VkImageViewCreateInfo depthViewCreateInfo = vkinit::imageview_create_info(_depthFormat, _depthImage._image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(_device, &depthViewCreateInfo, nullptr, &_depthImageView));

	_swapChainDeletionStack.push_function([=]() {
		vmaDestroyImage(_vmaAllocator, _depthImage._image, _depthImage._allocation);
		vkDestroyImageView(_device, _depthImageView, nullptr);
	});

	LOG_SUCCESS("Swapchain Initialization Complete");
}

void VulkanEngine::init_commands()
{
	LOG_INFO("Command Structures Initialization");

	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

		_swapChainDeletionStack.push_function([=]() {
			vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
			});
	}

	//mesh data uploading

	VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_meshUploadContext._commandPool));

	_swapChainDeletionStack.push_function([=]() {
		vkDestroyCommandPool(_device, _meshUploadContext._commandPool, nullptr);
		});

	LOG_SUCCESS("Command Structures Initialized");
}

void VulkanEngine::init_default_renderpass(const vkb::Swapchain& swapchain)
{
	LOG_INFO("Default Renderpass Initialization");

	// Attachment

	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapchain.image_format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentReference = {};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Depth Buffer

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.flags = 0;
	depthAttachment.format = _depthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentReference = {};
	depthAttachmentReference.attachment = 1;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// Subpass

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;
	subpass.pDepthStencilAttachment = &depthAttachmentReference;

	// RenderPass

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

	VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };

	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = &attachments[0];

	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	VK_CHECK(vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_renderPass));

	_swapChainDeletionStack.push_function([=]() {
		vkDestroyRenderPass(_device, _renderPass, nullptr);
	});

	LOG_SUCCESS("Default Renderpass Initialized");
}

void VulkanEngine::init_framebuffers()
{
	LOG_INFO("Framebuffer Initialization");

	VkFramebufferCreateInfo framebufferInfo = {};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.pNext = nullptr;

	framebufferInfo.renderPass = _renderPass;
	framebufferInfo.attachmentCount = 1;
	framebufferInfo.width = _windowExtent.width;
	framebufferInfo.height = _windowExtent.height;
	framebufferInfo.layers = 1;

	const size_t swapchain_image_count = _swapchainImages.size();
	_framebuffers = std::vector<VkFramebuffer>(swapchain_image_count);

	std::vector<VkResult> frameBufferResults{};

	for (int i = 0; i < swapchain_image_count; i++) {
		VkImageView attachments[2]{ _swapchainImageViews[i], _depthImageView };

		framebufferInfo.pAttachments = attachments;
		framebufferInfo.attachmentCount = 2;

		frameBufferResults.push_back(vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_framebuffers[i]));

		_swapChainDeletionStack.push_function([=]() {
			vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
		});
	}

	VK_CHECKS(frameBufferResults);
	LOG_SUCCESS("Framebuffers Initialized");
}

void VulkanEngine::init_sync_structures()
{
	LOG_INFO("Sync Structure Initialization");

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = nullptr;

	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.pNext = nullptr;
	semaphoreInfo.flags = 0;

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		FrameData& frame = _frames[i];

		std::vector<VkResult> results = {
			vkCreateFence(_device, &fenceInfo, nullptr, &frame._renderFence),
			vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &frame._presentSemaphore),
			vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &frame._renderSemaphore) };

		VK_CHECKS(results);

		_mainDeletionStack.push_function([=]() {
			vkDestroySemaphore(_device, frame._presentSemaphore, nullptr);
			vkDestroySemaphore(_device, frame._renderSemaphore, nullptr);
			vkDestroyFence(_device, frame._renderFence, nullptr);
			});
	}

	fenceInfo.flags = 0;

	VK_CHECK(vkCreateFence(_device, &fenceInfo, nullptr, &_meshUploadContext._uploadFence));

	_mainDeletionStack.push_function([=]() {
		vkDestroyFence(_device, _meshUploadContext._uploadFence, nullptr);
		});

	LOG_SUCCESS("Sync Structures Initialized");
}

void VulkanEngine::init_pipelines()
{
	LOG_INFO("Pipeline Initialization");

	PipelineBuilder pipelineBuilder;

	pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();
	pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();
	pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS);

	VertexInputDescription vertexDescription = Vertex::get_vertex_description();

	pipelineBuilder._vertexInputState = vkinit::vertex_input_state_create_info();
	pipelineBuilder._vertexInputState.pVertexAttributeDescriptions = vertexDescription.attributes.data();
	pipelineBuilder._vertexInputState.vertexAttributeDescriptionCount = vertexDescription.attributes.size();

	pipelineBuilder._vertexInputState.pVertexBindingDescriptions = vertexDescription.bindings.data();
	pipelineBuilder._vertexInputState.vertexBindingDescriptionCount = vertexDescription.bindings.size();

	ShaderModuleDescriptorSetLayout cameraLayout(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1);
	ShaderModuleDescriptorSetLayout sceneLayout(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1);
	ShaderModuleDescriptorSetLayout objectsLayout(1, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
	ShaderModuleDescriptorSetLayout textureLayout(2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);

	ShaderModule texturedLitFragment;
	std::vector<ShaderModuleDescriptorSetLayout> texturedLitFragmentDescriptors{
		sceneLayout, textureLayout
	};

	ShaderModule defaultLitFragment;
	std::vector<ShaderModuleDescriptorSetLayout> defaultLitFragmentDescriptors{
		sceneLayout
	};

	ShaderModule triangleMeshVertex;
	std::vector<ShaderModuleDescriptorSetLayout> triangleMeshVertexShaderDescriptors{
		cameraLayout, objectsLayout
	};

	std::vector<VkResult> results{
		ShaderModule::load(_device, "../shaders/textured_lit.frag.spv", texturedLitFragmentDescriptors, &texturedLitFragment),
		ShaderModule::load(_device, "../shaders/default_lit.frag.spv", defaultLitFragmentDescriptors, &defaultLitFragment),
		ShaderModule::load(_device, "../shaders/tri_mesh.vert.spv", triangleMeshVertexShaderDescriptors, &triangleMeshVertex)
	};

	VK_CHECKS(results);

	//Untextured mesh pipeline
	ShaderEffect meshPipelineEffect{};
	meshPipelineEffect.add_stage(triangleMeshVertex, VK_SHADER_STAGE_VERTEX_BIT);
	meshPipelineEffect.add_stage(defaultLitFragment, VK_SHADER_STAGE_FRAGMENT_BIT);

	VK_CHECK(meshPipelineEffect.build_layout(_device, _descriptorSetLayoutCache));

	pipelineBuilder.set_shaders(meshPipelineEffect);
	pipelineBuilder._pipelineLayout = meshPipelineEffect._builtLayout;

	VK_CHECK(pipelineBuilder.build_pipeline(_device, _renderPass, &_meshPipeline));

	create_material(_meshPipeline, meshPipelineEffect._builtLayout, "defaultmaterial");

	_mainDeletionStack.push_function([=]() {
			vkDestroyPipeline(_device, _meshPipeline, nullptr);
			vkDestroyPipelineLayout(_device, meshPipelineEffect._builtLayout, nullptr);
		});

	//Textured mesh pipeline
	ShaderEffect texturedMeshPipelineEffect{};
	texturedMeshPipelineEffect.add_stage(triangleMeshVertex, VK_SHADER_STAGE_VERTEX_BIT);
	texturedMeshPipelineEffect.add_stage(texturedLitFragment, VK_SHADER_STAGE_FRAGMENT_BIT);

	VK_CHECK(texturedMeshPipelineEffect.build_layout(_device, _descriptorSetLayoutCache));

	pipelineBuilder.set_shaders(texturedMeshPipelineEffect);
	pipelineBuilder._pipelineLayout = texturedMeshPipelineEffect._builtLayout;

	VkPipeline texturedMeshPipeline{};
	VK_CHECK(pipelineBuilder.build_pipeline(_device, _renderPass, &texturedMeshPipeline));

	create_material(texturedMeshPipeline, texturedMeshPipelineEffect._builtLayout, "texturedMesh");

	_mainDeletionStack.push_function([=]() {
		vkDestroyPipeline(_device, texturedMeshPipeline, nullptr);
		vkDestroyPipelineLayout(_device, texturedMeshPipelineEffect._builtLayout, nullptr);
		});

	//Destroy these immediately since they are not needed any longer
	vkDestroyShaderModule(_device, triangleMeshVertex._vkShaderModule, nullptr);
	vkDestroyShaderModule(_device, defaultLitFragment._vkShaderModule, nullptr);
	vkDestroyShaderModule(_device, texturedLitFragment._vkShaderModule, nullptr);

	LOG_SUCCESS("Pipelines Initialized");
}

FrameData& VulkanEngine::get_current_frame()
{
	return _frames[_frameNumber % FRAME_OVERLAP];
}

void VulkanEngine::load_images()
{
	LOG_INFO("Loading Images");

	Texture lostEmpire;

	vkutil::load_image_from_file(*this, "../assets/lost_empire-RGBA.png", lostEmpire.image);

	VkImageViewCreateInfo imageinfo = vkinit::imageview_create_info(VK_FORMAT_R8G8B8A8_SRGB, lostEmpire.image._image, VK_IMAGE_ASPECT_COLOR_BIT);
	VK_CHECK(vkCreateImageView(_device, &imageinfo, nullptr, &lostEmpire.imageView));

	_loadedTextures["empire_diffuse"] = lostEmpire;

	_mainDeletionStack.push_function([=]() {
		vkDestroyImageView(_device, lostEmpire.imageView, nullptr);
		});

	LOG_SUCCESS("Images Loaded");
}

void VulkanEngine::load_meshes()
{
	Mesh triangleMesh;
	triangleMesh._vertices.resize(3);

	triangleMesh._vertices[0].position = { 1.f, 1.f, 0.f };
	triangleMesh._vertices[1].position = { -1.f, 1.f, 0.f };
	triangleMesh._vertices[2].position = { 0.f, -1.f, 0.f };

	triangleMesh._vertices[0].color = { 1.f, 0.f, 0.f };
	triangleMesh._vertices[1].color = { 0.f, 1.f, 0.f };
	triangleMesh._vertices[2].color = { 0.f, 0.f, 1.f };

	Mesh monkeyMesh;
	Mesh::load_from_triangulated_obj("../assets/monkey_smooth.obj", monkeyMesh);

	Mesh empire;
	Mesh::load_from_triangulated_obj("../assets/lost_empire.obj", empire);

	upload_mesh(triangleMesh);
	upload_mesh(monkeyMesh);
	upload_mesh(empire);

	_meshes["empire"] = std::make_shared<Mesh>(empire);
	_meshes["monkey"] = std::make_shared<Mesh>(monkeyMesh);
	_meshes["triangle"] = std::make_shared<Mesh>(triangleMesh);
}

void VulkanEngine::upload_mesh(Mesh& pMesh)
{
	const auto bufferSize = pMesh._vertices.size() * sizeof(Vertex);

	//Create and write to staging buffer that stores mesh data CPU side before transfering

	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bufferSize;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

	vkutil::AllocatedBuffer stagingBuffer;

	VK_CHECK(vmaCreateBuffer(_vmaAllocator, &bufferInfo, &vmaallocInfo, &stagingBuffer._buffer, &stagingBuffer._allocation, nullptr));

	void* data;
	VK_CHECK(vmaMapMemory(_vmaAllocator, stagingBuffer._allocation, &data));
	memcpy(data, pMesh._vertices.data(), bufferSize);
	vmaUnmapMemory(_vmaAllocator, stagingBuffer._allocation);

	//Create GPU side buffer that will store the mesh data

	VkBufferCreateInfo vertexBufferInfo = {};
	vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexBufferInfo.pNext = nullptr;
	vertexBufferInfo.size = bufferSize;
	vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VK_CHECK(vmaCreateBuffer(_vmaAllocator, &vertexBufferInfo, &vmaallocInfo, &pMesh._vertexBuffer._buffer, &pMesh._vertexBuffer._allocation, nullptr));

	immediate_submit([=](VkCommandBuffer commandBuffer) {
		VkBufferCopy copy;
		copy.dstOffset = 0;
		copy.srcOffset = 0;
		copy.size = bufferSize;
		vkCmdCopyBuffer(commandBuffer, stagingBuffer._buffer, pMesh._vertexBuffer._buffer, 1, &copy);
		});

	_mainDeletionStack.push_function([=]() {
		vmaDestroyBuffer(_vmaAllocator, pMesh._vertexBuffer._buffer, pMesh._vertexBuffer._allocation);
		});

	vmaDestroyBuffer(_vmaAllocator, stagingBuffer._buffer, stagingBuffer._allocation);
}

void VulkanEngine::init_scene()
{
	LOG_INFO("Scene Initialization");

	RenderObject monkey;
	monkey.pMesh = get_mesh("monkey");
	monkey.pMaterial = get_material("defaultmaterial");
	monkey.transform = glm::mat4(1.0f);
	monkey.color = { 1.0f, 1.0f, 1.0f, 1.0f };

	_renderables.push_back(monkey);

	static fastrng rng(std::chrono::system_clock::now().time_since_epoch().count());

	for (int x = -20; x <= 20; x++) {
		for (int y = -20; y <= 20; y++) {
			RenderObject triangle;
			triangle.pMesh = get_mesh("triangle");
			triangle.pMaterial = get_material("defaultmaterial");
			glm::mat4 translation = glm::translate(glm::identity<glm::mat4>(), glm::vec3(x, 0, y));
			glm::mat4 scale = glm::scale(glm::identity<glm::mat4>(), glm::vec3(0.2, 0.2, 0.2));
			glm::vec4 color = { rng.get_double(), rng.get_double(), rng.get_double(), rng.get_double() };
			triangle.color = color;
			triangle.transform = translation * scale;

			_renderables.push_back(triangle);
		}
	}

	RenderObject map;
	map.pMesh = get_mesh("empire");
	map.pMaterial = get_material("texturedMesh");
	map.transform = glm::translate(glm::vec3{ 5, -10, 0 });

	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT);

	VkSampler blockySampler;
	VK_CHECK(vkCreateSampler(_device, &samplerInfo, nullptr, &blockySampler));

	std::shared_ptr<Material> texturedMat = get_material("texturedMesh");

	if (!_descriptorAllocator.allocate(&texturedMat->textureSet, _singleTextureSetLayout))
	{
		LOG_ERROR("Failed to allocate textured material descriptor set");
	}

	VkDescriptorImageInfo imageBufferInfo;
	imageBufferInfo.sampler = blockySampler;
	imageBufferInfo.imageView = _loadedTextures["empire_diffuse"].imageView;
	imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet texture1 = vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texturedMat->textureSet, imageBufferInfo, 0);

	vkUpdateDescriptorSets(_device, 1, &texture1, 0, nullptr);

	_renderables.push_back(map);

	_mainDeletionStack.push_function([=]() {
		vkDestroySampler(_device, blockySampler, nullptr);
		});

	LOG_SUCCESS("Scene Initialized");
}

void VulkanEngine::init_input()
{
	LOG_INFO("Input Initialization");

	_inputManager.init();
	_playerCamera = {};
	_playerCamera.position = { 0, 6, 5 };

	LOG_SUCCESS("Input Initialized");
}

void VulkanEngine::init_descriptors()
{
	LOG_INFO("Descriptors Initialization");

	_descriptorAllocator = {};
	_descriptorAllocator.init(_device);

	_descriptorSetLayoutCache = {};
	_descriptorSetLayoutCache.init(_device);

	//Layouts for pipeline

	VkDescriptorSetLayoutBinding textureBinding = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

	VkDescriptorSetLayoutCreateInfo textureSetInfo{};
	textureSetInfo.pNext = nullptr;
	textureSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

	textureSetInfo.bindingCount = 1;
	textureSetInfo.flags = 0;
	textureSetInfo.pBindings = &textureBinding;

	VK_CHECK(vkCreateDescriptorSetLayout(_device, &textureSetInfo, nullptr, &_singleTextureSetLayout));
	_mainDeletionStack.push_function([=]() {
		vkDestroyDescriptorSetLayout(_device, _singleTextureSetLayout, nullptr);
		});

	//Scene Parameters
	_sceneBuffer = vkutil::WriteBuffer::build_constant(
		FRAME_OVERLAP,
		sizeof(GPUSceneParameters),
		_gpuProperties.limits.minUniformBufferOffsetAlignment,
		_vmaAllocator,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU,
		&_mainDeletionStack);
	_cameraBuffer = vkutil::WriteBuffer::build_constant(
		FRAME_OVERLAP,
		sizeof(GPUCameraData),
		_gpuProperties.limits.minUniformBufferOffsetAlignment,
		_vmaAllocator,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU,
		&_mainDeletionStack);

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		constexpr uint64_t MAX_OBJECTS = 10000;
		constexpr uint32_t MAX_COMMANDS = 10000; //One command for each object before we have instancing

		FrameData& frame = _frames[i];

		frame._objectBuffer = vkutil::WriteBuffer::build_constant(
			1,
			MAX_OBJECTS * sizeof(GPUObjectData),
			_gpuProperties.limits.minStorageBufferOffsetAlignment,
			_vmaAllocator,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU,
			&_mainDeletionStack);

		frame._indirectDrawCommands = vkutil::WriteBuffer::build_constant(
			1,
			MAX_COMMANDS * sizeof(VkDrawIndirectCommand),
			_gpuProperties.limits.minStorageBufferOffsetAlignment,
			_vmaAllocator,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU,
			&_mainDeletionStack);

		//Connect the buffers

		VkDescriptorBufferInfo cameraInfo;
		cameraInfo.buffer = _cameraBuffer.buffer()._buffer;
		cameraInfo.offset = 0;
		cameraInfo.range = sizeof(GPUCameraData);

		VkDescriptorBufferInfo sceneInfo;
		sceneInfo.buffer = _sceneBuffer.buffer()._buffer;
		sceneInfo.offset = 0;
		sceneInfo.range = sizeof(GPUSceneParameters);

		VkDescriptorBufferInfo objectInfo;
		objectInfo.buffer = frame._objectBuffer.buffer()._buffer;
		objectInfo.offset = 0;
		objectInfo.range = MAX_OBJECTS * sizeof(GPUObjectData);

		DescriptorSetBuilder::begin(_descriptorSetLayoutCache, _descriptorAllocator)
			.bind_buffer(0, cameraInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
			.bind_buffer(1, sceneInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(frame._globalDescriptorSet);

		DescriptorSetBuilder::begin(_descriptorSetLayoutCache, _descriptorAllocator)
			.bind_buffer(0, objectInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
			.build(frame._objectDescriptorSet);
	}

	LOG_SUCCESS("Descriptors Initialized");
}

void VulkanEngine::draw()
{
	ImGui::Render();

	FrameData& currentFrame = get_current_frame();

	VK_CHECK(vkWaitForFences(_device, 1, &currentFrame._renderFence, true, (uint64_t)1e9)); //We set this fence when submitting the command buffer, since we must wait for it to go from pending to executable.
	VK_CHECK(vkResetFences(_device, 1, &currentFrame._renderFence));

	//Pre-render Pass

	uint32_t swapchainImageIndex;
	VkResult result = vkAcquireNextImageKHR(_device, _vkbSwapchain.swapchain, (uint64_t)1e9, currentFrame._presentSemaphore, nullptr, &swapchainImageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreate_swap_chain();
		return; //Dip out early since the commands in flight are out of date
	}
	else
	{
		VK_CHECK(result);
	}

	VkCommandBuffer commandBuffer = currentFrame._mainCommandBuffer;
	VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;

	commandBufferBeginInfo.pInheritanceInfo = nullptr;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo)); //Open the command buffer (recording state)
	
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)_windowExtent.width;
	viewport.height = (float)_windowExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = _windowExtent;

	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	//Background color

	VkClearValue clearValue;
	float flash = abs(sin(_frameNumber / 120.f));
	clearValue.color = { { 0.0f, 0.0f, flash, 1.0f } };

	VkClearValue depthClear;
	depthClear.depthStencil.depth = 1.f;

	//Render Pass

	VkRenderPassBeginInfo renderPassBeginInfo = vkinit::renderpass_begin_info(_renderPass, _windowExtent, _framebuffers[swapchainImageIndex]);
	renderPassBeginInfo.clearValueCount = 2;

	VkClearValue clearValues[] = { clearValue, depthClear };

	renderPassBeginInfo.pClearValues = &clearValues[0];

	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE); //Set the rendering into the command buffer

	draw_objects(commandBuffer, _renderables);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

	vkCmdEndRenderPass(commandBuffer);
	VK_CHECK(vkEndCommandBuffer(commandBuffer)); //Close the command buffer (executable state)

	//Enqueue command buffer

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	submitInfo.pWaitDstStageMask = &waitStage;

	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &currentFrame._presentSemaphore;

	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &currentFrame._renderSemaphore;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, currentFrame._renderFence)); //Submit the command buffer (pending state)

	//Present to screen

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;

	presentInfo.pSwapchains = &_vkbSwapchain.swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &currentFrame._renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	result = vkQueuePresentKHR(_graphicsQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		recreate_swap_chain();
	}
	else
	{
		VK_CHECK(result);
	}

	_frameNumber++;
}

VkResult VulkanEngine::load_shader_module(const std::string filePath, VkShaderModule* outShaderModule)
{
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

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

	return vkCreateShaderModule(_device, &shaderModuleCreateInfo, nullptr, outShaderModule);
}

void VulkanEngine::run()
{
	SDL_Event event;

	bool quit = false;

	//const uint64_t numberOfShaders = 2;

	const double cameraSpeed = 1; //world units per second

	auto lastUpdateStart = std::chrono::steady_clock::now();

	uint64_t elapsedFixedNanoseconds = 0;
	uint64_t elapsedUpdateNanoSeconds = 0;

	const uint64_t targetFixedDeltaNanoseconds = 1.0f / 60.0f * 1e9;
	const uint64_t targetUpdateDeltaNanoseconds = 1.0f / 120.0f * 1e9;

	while (!quit)
	{
		while (SDL_PollEvent(&event) != 0)
		{
			ImGui_ImplSDL2_ProcessEvent(&event);

			switch (event.type) {
			case SDL_QUIT:
				quit = true;
				break;
			}
		}
		
		if (_gameLoopInterrupted)
		{
			LOG_INFO("Game loop interrupted");
			_gameLoopInterrupted = false;
			lastUpdateStart = std::chrono::steady_clock::now();
			continue;
		}

		auto trueElapsedNanoseconds = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - lastUpdateStart).count();
		lastUpdateStart = std::chrono::steady_clock::now();

		if (trueElapsedNanoseconds > 2 * targetFixedDeltaNanoseconds)
		{
			LOG_INFO("Behind on fixed updates by {} seconds", (double)trueElapsedNanoseconds / 1e9);
		}

		elapsedFixedNanoseconds += trueElapsedNanoseconds;
		elapsedUpdateNanoSeconds += trueElapsedNanoseconds;

		//Do fixed updates
		if (elapsedFixedNanoseconds > targetFixedDeltaNanoseconds)
		{
			while (elapsedFixedNanoseconds > targetFixedDeltaNanoseconds)
			{
				//Fixed update loop

				//Camera Movement

				auto movementVector = _inputManager.getMovement(true);
				auto rotateVector = _inputManager.getMouseDelta();

				_playerCamera.yaw += rotateVector.x / (float)_windowExtent.height;

				_playerCamera.pitch += rotateVector.y / (float)_windowExtent.height;
				_playerCamera.pitch = std::clamp(_playerCamera.pitch, -glm::radians(89.9f), glm::radians(89.9f));

				//Not orthogonal, but where movement axes should map to
				glm::vec3 cameraRight = _playerCamera.get_right({ 0,1,0 });
				glm::vec3 cameraUp = { 0, 1, 0 };
				glm::vec3 cameraForward = _playerCamera.get_forward();

				glm::mat3 cameraAxes{};
				cameraAxes[0] = cameraRight;
				cameraAxes[1] = cameraUp;
				cameraAxes[2] = cameraForward;
				   
				_playerCamera.velocity = (float)cameraSpeed * (cameraAxes * movementVector);
				_playerCamera.update((double)targetFixedDeltaNanoseconds / 1e9);

				elapsedFixedNanoseconds -= targetFixedDeltaNanoseconds;
			}

			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplSDL2_NewFrame(_window);
			ImGui::NewFrame();

			ImGui::ShowDemoWindow();

			draw(); //One draw call per flurry of fixed updates
		}

		//Variable update
	}

	return;
}

void VulkanEngine::cleanup_swapchain()
{
	_swapChainDeletionStack.flush();
}

void VulkanEngine::cleanup()
{
	if (_isInitialized) {
		for (int f = 0; f < FRAME_OVERLAP; f++)
		{
			VK_CHECK(vkWaitForFences(_device, 1, &_frames[f]._renderFence, true, (uint64_t)1e9)); //Wait for rendering to finish
		}

		_descriptorAllocator.cleanup();
		_descriptorSetLayoutCache.cleanup();

		cleanup_swapchain();

		_mainDeletionStack.flush();

		vmaDestroyAllocator(_vmaAllocator);

		vkDestroySurfaceKHR(_instance, _surface, nullptr);

		vkDestroyDevice(_device, nullptr);
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);

		SDL_DestroyWindow(_window);
	}
}

std::shared_ptr<Material> VulkanEngine::create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
{
	auto matPtr = std::make_shared<Material>();
	matPtr->pipeline = pipeline;
	matPtr->pipelineLayout = layout;
	_materials[name] = matPtr;
	return matPtr;
}

std::shared_ptr<Material> VulkanEngine::get_material(const std::string& name)
{
	auto index = _materials.find(name);
	if (index == _materials.end())
	{
		LOG_ERROR("Attempted to load material [{}] but could not find it", name);
		return nullptr;
	}
	else
	{
		return index->second;
	}
}

std::shared_ptr<Mesh> VulkanEngine::get_mesh(const std::string& name)
{
	auto index = _meshes.find(name);
	if (index == _meshes.end())
	{
		LOG_ERROR("Attempted to load mesh [{}] but could not find it", name);
		return nullptr;
	}
	else
	{
		return index->second;
	}
}

struct IndirectBatch {
	std::shared_ptr<Mesh> pMesh;
	std::shared_ptr<Material> pMaterial;
	uint32_t first; //Index into the object buffer
	uint32_t count;
};
std::vector<IndirectBatch> compact_draws(const std::vector<RenderObject>& objects)
{
	std::vector<IndirectBatch> draws;

	IndirectBatch firstDraw;
	firstDraw.pMesh = objects[0].pMesh;
	firstDraw.pMaterial = objects[0].pMaterial;
	firstDraw.first = 0;
	firstDraw.count = 1;

	draws.push_back(firstDraw);

	for (int i = 0; i < objects.size(); i++)
	{
		bool sameMesh = (objects[i].pMesh == draws.back().pMesh);
		bool sameMaterial = (objects[i].pMaterial == draws.back().pMaterial);

		if (sameMesh && sameMaterial)
		{
			draws.back().count++;
		}
		else
		{
			IndirectBatch newDraw;
			newDraw.pMesh = objects[i].pMesh;
			newDraw.pMaterial = objects[i].pMaterial;
			newDraw.first = i;
			newDraw.count = 1;

			draws.push_back(newDraw);
		}
	}
	return draws;
}

void VulkanEngine::draw_objects(VkCommandBuffer commandBuffer, const std::vector<RenderObject>& renderables)
{
	FrameData& currentFrame = get_current_frame();
	size_t frameIndex = _frameNumber % FRAME_OVERLAP;

	//Camera
	GPUCameraData camData;
	camData.viewproj = _playerCamera.get_projview_matrix((float)_windowExtent.width / (float)_windowExtent.height);

	_cameraBuffer.open(_vmaAllocator, frameIndex);
	_cameraBuffer.write(camData, 0);
	_cameraBuffer.close();

	//Environment

	float framed = (_frameNumber / 120.f);
	_sceneParameters.ambientColor = { sin(framed), 0, cos(framed), 1 };

	_sceneBuffer.open(_vmaAllocator, frameIndex);
	_sceneBuffer.write(_sceneParameters, 0);
	_sceneBuffer.close();

	//Objects

	//Write the renderables into the objectdata buffer
	currentFrame._objectBuffer.open(_vmaAllocator, 0);

	for (int i = 0; i < renderables.size(); i++)
	{
		const RenderObject& object = renderables[i];
		GPUObjectData objectData{};

		objectData.modelMatrix = object.transform;
		objectData.color = object.color;

		currentFrame._objectBuffer.write(objectData, i);
	}

	currentFrame._objectBuffer.close();

	//Write a drawcommand for every renderable

	currentFrame._indirectDrawCommands.open(_vmaAllocator, 0);
	for (int i = 0; i < renderables.size(); i++)
	{
		const RenderObject& object = renderables[i];
		VkDrawIndirectCommand drawCommand{};

		drawCommand.vertexCount = object.pMesh->_vertices.size();
		drawCommand.instanceCount = 1;
		drawCommand.firstVertex = 0;
		drawCommand.firstInstance = i;

		currentFrame._indirectDrawCommands.write(drawCommand, i);
	}
	currentFrame._indirectDrawCommands.close();

	std::shared_ptr<Mesh> pLastMesh = nullptr;
	std::shared_ptr<Material> pLastMaterial = nullptr;

	std::vector<IndirectBatch> draws = compact_draws(renderables);
	for (IndirectBatch& draw : draws)
	{
		if (draw.pMaterial != pLastMaterial) {
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.pMaterial->pipeline);
			pLastMaterial = draw.pMaterial;

			size_t sceneDataUniformOffset = _sceneBuffer.getOffset(frameIndex);
			size_t cameraDataUniformOffset = _cameraBuffer.getOffset(frameIndex);

			uint32_t offsets[2] = { sceneDataUniformOffset, cameraDataUniformOffset };

			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.pMaterial->pipelineLayout, 0, 1, &currentFrame._globalDescriptorSet, 2, offsets);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.pMaterial->pipelineLayout, 1, 1, &currentFrame._objectDescriptorSet, 0, nullptr);

			if (draw.pMaterial->textureSet != VK_NULL_HANDLE) { //TODO: refactor material system to get rid of this hardcoding
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.pMaterial->pipelineLayout, 2, 1, &draw.pMaterial->textureSet, 0, nullptr);
			}
		}

		if (draw.pMesh != pLastMesh) {
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &draw.pMesh->_vertexBuffer._buffer, &offset);
			pLastMesh = draw.pMesh;
		}

		VkDeviceSize indirectOffset = draw.first * sizeof(VkDrawIndirectCommand);
		VkDeviceSize drawStride = sizeof(VkDrawIndirectCommand);

		vkCmdDrawIndirect(commandBuffer, currentFrame._indirectDrawCommands.buffer()._buffer, indirectOffset, draw.count, drawStride);
	}
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer commandBuffer)>&& function)
{
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = vkinit::command_buffer_allocate_info(_meshUploadContext._commandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	VkCommandBuffer meshUploadCommandBuffer;
	VK_CHECK(vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo, &meshUploadCommandBuffer));

	VkCommandBufferBeginInfo commandBufferBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(meshUploadCommandBuffer, &commandBufferBeginInfo));

	function(meshUploadCommandBuffer);

	VK_CHECK(vkEndCommandBuffer(meshUploadCommandBuffer));

	std::vector<VkCommandBuffer> commandBuffers{ meshUploadCommandBuffer };
	VkSubmitInfo submitInfo = vkinit::submit_info(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, {}, {}, commandBuffers);

	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _meshUploadContext._uploadFence));

	VK_CHECK(vkWaitForFences(_device, 1, &_meshUploadContext._uploadFence, true, 1e9));
	VK_CHECK(vkResetFences(_device, 1, &_meshUploadContext._uploadFence));

	VK_CHECK(vkResetCommandPool(_device, _meshUploadContext._commandPool, 0));
}

