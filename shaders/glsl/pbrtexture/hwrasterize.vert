/*
 * Hardware Rasterization Vertex Shader
 * For visibility buffer generation pass
 */
#version 450

//=============================================================================
// Vertex Attributes
//=============================================================================
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec4 inClusterInfo;
layout(location = 5) in vec4 inClusterGroupInfo;

//=============================================================================
// Vertex Outputs
//=============================================================================
layout(location = 0) out vec3 outPosition;

//=============================================================================
// Main
//=============================================================================

void main()
{
    outPosition = inPosition;
}
