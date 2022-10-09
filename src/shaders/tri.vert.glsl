#version 460

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 color;

layout (location = 0) out vec3 outColor;

layout (set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
} cameraData;

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
}
