#version 460

layout (location = 0) in vec2 inUv;

layout (location = 0) out vec4 outColor;

layout (set = 2, binding = 0) uniform sampler2D tex;

void main() 
{
    vec3 color = texture(tex, inUv).rgb;
    outColor = vec4(color, 1.0f);
    //outColor = vec4(1.0, 1.0, 0.0, 1.0);
}
