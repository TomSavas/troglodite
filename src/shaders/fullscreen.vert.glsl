#version 460

layout (location = 0) out vec2 outUv;

// Damn this is smart: https://www.saschawillems.de/blog/2016/08/13/vulkan-tutorial-on-rendering-a-fullscreen-quad-without-buffers/
void main()
{
    outUv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUv * 2.f - 1.f, 0.f, 1.f);
}
