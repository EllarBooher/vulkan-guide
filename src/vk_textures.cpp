#include "vk_textures.h"
#include <iostream>
#include <vector>

#include "logger.h"

#include "vk_initializers.h"
#include "vk_buffers.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

bool vkutil::load_image_from_file(VulkanEngine& engine, std::string fileName, AllocatedImage& outImage)
{
	int32_t textureWidth, textureHeight, textureChannels;

	stbi_uc* pPixels = stbi_load(fileName.data(), &textureWidth, &textureHeight, &textureChannels, STBI_rgb_alpha);

	if (!pPixels) {
		std::cout << "Failed to load texture file at: " << fileName << std::endl;
		return false;
	}

	size_t imageSize = textureWidth * textureHeight * 4; //size of texture-to-be in bytes
	VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB; //match STB RGBA

	AllocatedBuffer stagingBuffer = AllocatedBuffer::create(
		imageSize, 
		engine._vmaAllocator,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VMA_MEMORY_USAGE_CPU_ONLY, 
		nullptr);

	void* data;
	VK_CHECK(vmaMapMemory(engine._vmaAllocator, stagingBuffer._allocation, &data));
	memcpy(data, pPixels, imageSize);
	vmaUnmapMemory(engine._vmaAllocator, stagingBuffer._allocation);

	stbi_image_free(pPixels);

	VkExtent3D imageExtent;
	imageExtent.width = textureWidth;
	imageExtent.height = textureHeight;
	imageExtent.depth = 1;

	VkImageCreateInfo depthImageInfo = vkinit::image_create_info(imageFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);

	AllocatedImage newImage;

	VmaAllocationCreateInfo depthImageAllocationInfo = {};

	depthImageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VK_CHECK(vmaCreateImage(engine._vmaAllocator, &depthImageInfo, &depthImageAllocationInfo, &newImage._image, &newImage._allocation, nullptr));

	engine.immediate_submit([&](VkCommandBuffer commandBuffer) {
		VkImageSubresourceRange range;
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = 1;
		range.baseArrayLayer = 0;
		range.layerCount = 1;

		VkImageMemoryBarrier imageBarrierToTransfer = {};
		imageBarrierToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrierToTransfer.pNext = nullptr;

		imageBarrierToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrierToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

		imageBarrierToTransfer.image = newImage._image;
		imageBarrierToTransfer.subresourceRange = range;

		imageBarrierToTransfer.srcAccessMask = 0;
		imageBarrierToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrierToTransfer);

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = imageExtent;

		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer._buffer, newImage._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		VkImageMemoryBarrier imageBarrierToReadable = imageBarrierToTransfer;
		imageBarrierToReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrierToReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		imageBarrierToReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageBarrierToReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrierToReadable);
		});

	engine._mainDeletionStack.push_function([=]() {
		vmaDestroyImage(engine._vmaAllocator, newImage._image, newImage._allocation);
		});

	vmaDestroyBuffer(engine._vmaAllocator, stagingBuffer._buffer, stagingBuffer._allocation);

	outImage = newImage;
	return true;
}