#pragma once

#include "glm/glm.hpp"

class InputManager {
public:
	void init();
	glm::vec3 getMovement(bool normalize = true);
	glm::vec<2, int32_t>  getMouseDelta();
private:
};