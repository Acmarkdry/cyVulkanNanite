/*
 * Hardware Rasterization Fragment Shader
 * Writes visibility ID to visibility buffer
 */
#version 450
#extension GL_GOOGLE_include_directive:enable
#include "data_structures.glsl"

//=============================================================================
// Fragment Inputs
//=============================================================================
layout(location = 0) in flat uint inVisibilityId;

//=============================================================================
// Fragment Outputs
//=============================================================================
layout(location = 0) out uvec4 outVisibility;

//=============================================================================
// Main
//=============================================================================

void main()
{
    outVisibility.x = inVisibilityId;
}
