#include "vk_mesh.h"
#include <tiny_obj_loader.h>
#include <iostream>

#include "logger.h"

VertexInputDescription Vertex::get_vertex_description()
{
	VertexInputDescription description = {};

	VkVertexInputBindingDescription mainBinding = {};
	mainBinding.binding = 0;
	mainBinding.stride = sizeof(Vertex);
	mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	description.bindings.push_back(mainBinding);

	VkVertexInputAttributeDescription positionAttribute = {};
	positionAttribute.binding = 0;
	positionAttribute.location = 0;
	positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	positionAttribute.offset = offsetof(Vertex, position);

	VkVertexInputAttributeDescription normalAttribute = {};
	normalAttribute.binding = 0;
	normalAttribute.location = 1;
	normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	normalAttribute.offset = offsetof(Vertex, normal);

	VkVertexInputAttributeDescription colorAttribute = {};
	colorAttribute.binding = 0;
	colorAttribute.location = 2;
	colorAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	colorAttribute.offset = offsetof(Vertex, color);


	VkVertexInputAttributeDescription uvAttribute = {};
	uvAttribute.binding = 0;
	uvAttribute.location = 3;
	uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
	uvAttribute.offset = offsetof(Vertex, uv);

	description.attributes.push_back(positionAttribute);
	description.attributes.push_back(normalAttribute);
	description.attributes.push_back(colorAttribute);
	description.attributes.push_back(uvAttribute);
	
	return description;
}

bool Mesh::load_from_triangulated_obj(const std::string& filename, Mesh& mesh)
{
	tinyobj::attrib_t attribute;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warning;
	std::string error;

	tinyobj::LoadObj(&attribute, &shapes, &materials, &warning, &error, filename.data(), "../assets");

	if (!warning.empty()) {
		LOG_WARNING("TINYOBJ WARNING: {}", warning);
	}

	if (!error.empty()) {
		LOG_ERROR("TINYOBJ ERROR: {}", error);
		return false;
	}

	//Iterate over shapes
	for (size_t s = 0; s < shapes.size(); s++) {
		size_t indexOffset = 0;

		//Iterate over faces
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			uint32_t faceVerticesCount = 3; //Hardcoded assumption of triangulation

			//Iterate over vertices of face
			for (size_t v = 0; v < faceVerticesCount; v++) {
				tinyobj::index_t index = shapes[s].mesh.indices[indexOffset + v];

				Vertex vertex;
				vertex.position.x = attribute.vertices[3 * index.vertex_index + 0];
				vertex.position.y = attribute.vertices[3 * index.vertex_index + 1];
				vertex.position.z = attribute.vertices[3 * index.vertex_index + 2];

				vertex.normal.x = attribute.normals[3 * index.normal_index + 0];
				vertex.normal.y = attribute.normals[3 * index.normal_index + 1];
				vertex.normal.z = attribute.normals[3 * index.normal_index + 2];

				tinyobj::real_t ux = attribute.texcoords[2 * index.texcoord_index + 0];
				tinyobj::real_t uy = attribute.texcoords[2 * index.texcoord_index + 1];

				vertex.uv.x = ux;
				vertex.uv.y = 1 - uy; //Vulkan UV coordinates are flipped

				vertex.color = vertex.normal;

				mesh._vertices.push_back(vertex);
			}

			indexOffset += faceVerticesCount;
		}
	}

	return true;
}
