#version 460

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 color;
layout (location = 3) in vec2 uv;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outUv;

layout (set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;

    //mat4 debugCamView;
    //mat4 debugCamProjection;
    //mat4 debugCamViewProjection;
} cameraData;

layout (set = 0, binding = 1) uniform SceneParams {
    vec4 fogColor; // w exponent
    vec4 fogDistances; // x for min, y for max, zw unused
    vec4 ambientColor;
    vec4 sunlightDirection; // w for sun power
    vec4 sunlightColor;
} sceneParams;

layout (push_constant) uniform constants
{
    vec4 data;
    mat4 modelMatrix;
} pushConstants;

struct ObjectData {
    mat4 modelMatrix;
};

layout (std140, set = 1, binding = 0) readonly buffer ObjectDataBuffer {
    ObjectData[] data;
} objectBuffer;

void main()
{
    mat4 mvp = cameraData.viewProjection * objectBuffer.data[gl_BaseInstance].modelMatrix;
    gl_Position = mvp * vec4(position, 1.0f);
    outColor = color;
    outUv = uv;
}
