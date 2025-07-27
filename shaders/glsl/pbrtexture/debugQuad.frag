#version 450

layout(location = 0) in vec2 inUV;
layout(binding = 0) uniform sampler2D tex;
layout(location = 0) out vec4 outputColor;

void main()
{
    vec3 color = textureLod(tex, inUV, 0).rgb;
    outputColor = vec4(color, 1.0);
}