#version 460

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUv;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 1) uniform SceneParams {
    vec4 fogColor; // w exponent
    vec4 fogDistances; // x for min, y for max, zw unused
    vec4 ambientColor;
    vec4 sunlightDirection; // w for sun power
    vec4 sunlightColor;
} sceneParams;

layout (set = 2, binding = 0) uniform sampler2D tex;

void main()
{
    vec3 color = texture(tex, inUv).rgb;
    outFragColor = vec4(color.rgb, 1.0f);
}

