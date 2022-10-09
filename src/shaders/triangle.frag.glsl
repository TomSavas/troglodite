//glsl version 4.5
#version 450

//output write
layout (location = 0) in vec3 inColor;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 1) uniform SceneParams {
    vec4 fogColor; // w exponent
    vec4 fogDistances; // x for min, y for max, zw unused
    vec4 ambientColor;
    vec4 sunlightDirection; // w for sun power
    vec4 sunlightColor;
} sceneParams;

void main()
{
    outFragColor = vec4(inColor + sceneParams.ambientColor.rgb, 1.0f);
}

