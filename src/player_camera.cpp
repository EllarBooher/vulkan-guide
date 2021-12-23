#include "player_camera.h"

void PlayerCamera::update(float deltaTime)
{
	position += velocity * deltaTime;
}

glm::mat4 PlayerCamera::get_projview_matrix(float aspectRatio)
{
	glm::mat4 view = glm::inverse(glm::translate(glm::mat4(1), position) * get_localToWorld_rotation_matrix());

	glm::mat4 projection = glm::perspective(glm::radians(90.f), aspectRatio, 0.1f, 5000.0f);
	projection[1][1] *= -1;

	return projection * view;
}

glm::mat4 PlayerCamera::get_localToWorld_rotation_matrix()
{
	glm::mat4 yaw_rotation = glm::rotate(glm::mat4{ 1 }, yaw, { 0, -1, 0 });
	glm::mat4 pitch_rotation = glm::rotate(yaw_rotation, pitch, { -1, 0, 0 });

	return pitch_rotation;
}

glm::vec3 PlayerCamera::get_forward()
{
	return (get_localToWorld_rotation_matrix() * glm::vec4(0, 0, -1, 0));
}

glm::vec3 PlayerCamera::get_right(glm::vec3 up)
{
	glm::vec3 trueRight = (get_localToWorld_rotation_matrix() * glm::vec4(1, 0, 0, 0));

	return trueRight - glm::proj(trueRight, up);
}