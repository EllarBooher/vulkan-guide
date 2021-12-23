#pragma once

#include "glm/glm.hpp"
#include "glm/gtx/transform.hpp"
#include "glm/gtx/projection.hpp"

struct PlayerCamera {
	glm::vec3 position;
	glm::vec3 velocity;
	
	float yaw;
	float pitch;

	bool bSprint;

	void update(float deltaTime);

	glm::vec3 get_forward();
	glm::vec3 get_right(glm::vec3 up);
	glm::mat4 get_localToWorld_rotation_matrix();
	glm::mat4 get_projview_matrix(float aspectRatio);
};