#include "input_manager.h"
#include "SDL.h"
#include "SDL_scancode.h"
#include <vector>
#include <iostream>

void InputManager::init()
{
	return;
	if (SDL_SetRelativeMouseMode(SDL_bool::SDL_TRUE) == -1)
	{
		std::cout << "Relative mouse mode not supported." << std::endl;
	}
}

glm::vec3 InputManager::getMovement(bool normalize)
{
	int numberOfKeys;
	const uint8_t* firstKey = SDL_GetKeyboardState(&numberOfKeys);

	std::vector<uint8_t> keys(firstKey, firstKey + numberOfKeys);

	glm::vec3 movement{};

	// Get if each key is held down

	bool forwardKey = keys[SDL_SCANCODE_W];
	bool backwardKey = keys[SDL_SCANCODE_S];
	bool leftKey = keys[SDL_SCANCODE_A];
	bool rightKey = keys[SDL_SCANCODE_D];
	bool upKey = keys[SDL_SCANCODE_Q];
	bool downKey = keys[SDL_SCANCODE_E];

	// Get if that key was held down previously. Desired behavior: for two conflicting keys (i.e. up and down), the last one the player hit is the direction
	// that shall be inputted.

	if (rightKey)
	{
		movement.x = 1.0f;
	}
	else if (leftKey)
	{
		movement.x = -1.0f;
	}
	if (upKey)
	{
		movement.y = 1.0f;
	}
	else if (downKey)
	{
		movement.y = -1.0f;
	}
	if (forwardKey)
	{
		movement.z = 1.0f;
	}
	else if (backwardKey)
	{
		movement.z = -1.0f;
	}

	if (glm::length(movement) < 0.001) { return glm::vec4(0, 0, 0, 1); }

	glm::vec3 returnVector = normalize ? glm::normalize(movement) : movement;

	return returnVector;
}

glm::vec<2, int32_t> InputManager::getMouseDelta()
{
	int32_t xDelta, yDelta;

	SDL_GetRelativeMouseState(&xDelta, &yDelta);

	return { xDelta, yDelta };
}