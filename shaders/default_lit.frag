#version 450

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 texCoord;

layout (location = 0) out vec4 outFragColor;

layout(set = 0, binding = 1) uniform SceneData{
	vec4 fogColor; //r g b exponent
	vec4 fogDistances; //x min, y max, zw unused
	vec4 ambientColor; //r g b exponent
	vec4 sunlightDirection; //x y z power
	vec4 sunlightColor; //r g b exponent
} sceneData;

void main()
{	
	outFragColor = vec4(inColor, 1.0f);
}