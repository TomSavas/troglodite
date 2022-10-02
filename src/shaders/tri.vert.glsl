//we will be using glsl version 4.5 syntax
#version 450

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

void main()
{
    //output the position of each vertex
    gl_Position = cameraData.viewProjection * pushConstants.modelMatrix * vec4(position, 1.0f);
    outColor = color;
}
