//Lightweight wrappers to buffers so one does not need to worry about alignment too much

#pragma once

#include "vk_types.h"
#include "logger.h"

namespace vkutil
{
	struct AllocatedBuffer {
		VkBuffer _buffer;
		VmaAllocation _allocation;
		VkDeviceSize _allocSize{ 0 };
		VkDescriptorBufferInfo build_info(VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);
		static AllocatedBuffer create(
			size_t allocSize, 
			VmaAllocator allocator, 
			VkBufferUsageFlags usage, 
			VmaMemoryUsage memoryUsage, 
			DeletionStack* pDeletionStack);
	};

	class WriteBuffer
	{
	public:
		/// <summary>
		/// Build a buffer that stores multiple sub-buffers of variable size. The offset of each sub-buffer will conform with the restrictions given in minUniformBufferOffsetAlignment.
		/// </summary>
		/// <param name="sizes">A vector that stores the size in bytes of each sub-buffer.</param>
		/// <param name="minUniformBufferOffsetAlignment">The minimum offset alignment that each sub-buffer should conform to.</param>
		/// <param name="pDeletionStack">Deletion stack to store cleanup of this buffer.</param>
		/// <returns></returns>
		static WriteBuffer build_variable(
			std::vector<size_t> sizes,
			size_t minUniformBufferOffsetAlignment,
			VmaAllocator allocator, 
			VkBufferUsageFlags usage, 
			VmaMemoryUsage memoryUsage,
			DeletionStack *pDeletionStack);

		/// <summary>
		/// Build a buffer that stores a single size sub-buffer, but multiple of them. The offset of each sub-buffer will conform with the restrictions given in minUniformBufferOffsetAlignment.
		/// </summary>
		/// <param name="count">How many sub-buffers.</param>
		/// <param name="size">The size of each in bytes.</param>
		/// <param name="pDeletionStack">Deletion stack to store cleanup of this buffer.</param>
		/// <returns></returns>
		static WriteBuffer build_constant(
			size_t count,
			size_t size,
			size_t minUniformBufferOffsetAlignment,
			VmaAllocator allocator,
			VkBufferUsageFlags usage,
			VmaMemoryUsage memoryUsage,
			DeletionStack* pDeletionStack);

		void open(VmaAllocator& allocator, size_t subBuffer);
		template<typename T>
		void write(
			T& data, 
			size_t position);
		void close();

		size_t getOffset(size_t subBuffer) const { return _subBufferOffsets[subBuffer]; };

		const AllocatedBuffer& buffer() const { return _buffer; }

	private:
		AllocatedBuffer _buffer{};
		std::vector<size_t> _subBufferOffsets{};
		std::vector<size_t> _subBufferEnds{};

		VmaAllocator* _pAllocator = nullptr;
		size_t _openSubBuffer = 0;
		char* _pData = nullptr;
	};

	//Not finished
	class PushBuffer
	{
	public:
		template<typename T>
		void push(T& data) {};
		void reset() {};
	private:
		AllocatedBuffer _buffer;
	};
}

namespace vkutil
{
	template<typename T>
	void WriteBuffer::write(
		T& data,
		size_t position)
	{
		if (_pData == nullptr || _pAllocator == nullptr)
		{
			LOG_ERROR("Writing to WriteBuffer that is not open");
			return;
		}

		size_t offset = sizeof(T) * position + _subBufferOffsets[_openSubBuffer];

		if (offset + sizeof(T) > _subBufferEnds[_openSubBuffer])
		{
			LOG_ERROR("Writing to position of WriteBuffer's Sub-Buffer that is out of bounds");
			return;
		}

		char* writePosition = _pData + offset;
		size_t writeSize = sizeof(T);

		memcpy(writePosition, &data, writeSize);
	}
}