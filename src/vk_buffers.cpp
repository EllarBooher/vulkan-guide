#include "vk_buffers.h"

inline size_t pad_uniform_buffer_size(size_t originalSize, size_t minUniformBufferOffsetAlignment)
{
	size_t minUniformBufferOffsetAllignment = minUniformBufferOffsetAlignment;
	size_t alignedSize = originalSize;

	if (alignedSize / minUniformBufferOffsetAllignment * minUniformBufferOffsetAlignment == alignedSize)
	{
		return alignedSize;
	}

	if (minUniformBufferOffsetAllignment > 0) {
		auto multiples = (originalSize - 1) / minUniformBufferOffsetAllignment + 1;
		alignedSize = multiples * minUniformBufferOffsetAllignment;
	}
	return alignedSize;
}

namespace vkutil
{
	AllocatedBuffer AllocatedBuffer::create(
		size_t allocSize,
		VmaAllocator allocator,
		VkBufferUsageFlags usage,
		VmaMemoryUsage memoryUsage,
		DeletionStack* pDeletionStack)
	{
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.pNext = nullptr;

		bufferInfo.size = allocSize;
		bufferInfo.usage = usage;

		VmaAllocationCreateInfo vmaallocInfo{};
		vmaallocInfo.usage = memoryUsage;

		AllocatedBuffer newBuffer;

		VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaallocInfo, &newBuffer._buffer, &newBuffer._allocation, nullptr));

		if (pDeletionStack != nullptr)
		{
			pDeletionStack->push_function([=]() {
				vmaDestroyBuffer(allocator, newBuffer._buffer, newBuffer._allocation);
				});
		}

		newBuffer._allocSize = allocSize;

		return newBuffer;
	}

	VkDescriptorBufferInfo AllocatedBuffer::build_info(VkDeviceSize offset, VkDeviceSize range)
	{
		VkDescriptorBufferInfo info{};
		info.buffer = _buffer;
		info.offset = offset;
		info.range = range;

		return info;
	}

	WriteBuffer WriteBuffer::build_variable(
		std::vector<size_t> sizes,
		size_t minUniformBufferOffsetAlignment,
		VmaAllocator allocator,
		VkBufferUsageFlags usage,
		VmaMemoryUsage memoryUsage,
		DeletionStack* pDeletionStack)
	{
		WriteBuffer writeBuffer{};
		writeBuffer._subBufferOffsets.clear();
		writeBuffer._subBufferEnds.clear();

		size_t currentOffset = 0;
		for (auto size : sizes)
		{
			size_t offset = pad_uniform_buffer_size(currentOffset, minUniformBufferOffsetAlignment);
			writeBuffer._subBufferOffsets.push_back(offset);
			writeBuffer._subBufferEnds.push_back(offset + size);

			currentOffset = offset + size;
		}

		size_t allocSize = currentOffset;

		AllocatedBuffer buffer = AllocatedBuffer::create(
			allocSize,
			allocator,
			usage,
			memoryUsage,
			pDeletionStack
		);

		writeBuffer._buffer = buffer;

		return writeBuffer;
	}

	WriteBuffer WriteBuffer::build_constant(
		size_t count,
		size_t size,
		size_t minUniformBufferOffsetAlignment,
		VmaAllocator allocator,
		VkBufferUsageFlags usage,
		VmaMemoryUsage memoryUsage,
		DeletionStack* pDeletionStack)
	{
		WriteBuffer writeBuffer{};
		writeBuffer._subBufferOffsets.clear();
		writeBuffer._subBufferEnds.clear();

		size_t currentOffset = 0;
		for (int i = 0; i < count; i++)
		{
			size_t offset = pad_uniform_buffer_size(currentOffset, minUniformBufferOffsetAlignment);
			writeBuffer._subBufferOffsets.push_back(offset);
			writeBuffer._subBufferEnds.push_back(offset + size);

			currentOffset = offset + size;
		}

		size_t allocSize = currentOffset;

		AllocatedBuffer buffer = AllocatedBuffer::create(
			allocSize,
			allocator,
			usage,
			memoryUsage,
			pDeletionStack
		);

		writeBuffer._buffer = buffer;

		return writeBuffer;
	}

	void WriteBuffer::open(VmaAllocator& allocator, size_t subBuffer)
	{
		if (_pData != nullptr || _pAllocator != nullptr)
		{
			LOG_WARNING("Opening WriteBuffer that is already open");
			return;
		}

		void* pData;

		VK_CHECK(vmaMapMemory(allocator, _buffer._allocation, &pData));

		_pData = (char*)pData;
		_pAllocator = &allocator;

		_openSubBuffer = subBuffer;
	}

	void WriteBuffer::close()
	{
		if (_pData == nullptr || _pAllocator == nullptr)
		{
			LOG_WARNING("Closing WriteBuffer that is already closed");
			return;
		}

		vmaUnmapMemory(*_pAllocator, _buffer._allocation);

		_pData = nullptr;
		_pAllocator = nullptr;
	}
}