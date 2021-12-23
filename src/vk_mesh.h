#pragma once

#include "vk_types.h"
#include <vector>
#include <glm/vec3.hpp>
#include <string>

struct VertexInputDescription;

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 uv;

	static VertexInputDescription get_vertex_description();
};

struct Mesh {
	std::vector<Vertex> _vertices;
	AllocatedBuffer _vertexBuffer;

	static bool load_from_triangulated_obj(const std::string& filename, Mesh& mesh);
};

struct VertexInputDescription {
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;

	VkPipelineVertexInputStateCreateFlags flags = 0;
};