/*
 * Forward PBR Vertex Shader
 * Passes vertex data to geometry shader for transformation
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
// Vertex Outputs (to Geometry Shader)
//=============================================================================
layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out vec4 outTangent;
layout(location = 4) out vec4 outClusterInfo;
layout(location = 5) out vec4 outClusterGroupInfo;

//=============================================================================
// Uniform Buffers
//=============================================================================
layout(binding = 0) uniform CameraUBO {
    mat4 projection;
    mat4 model;
    mat4 view;
    vec3 cameraPos;
} camera;

layout(binding = 10) readonly buffer ObjectIdBuffer {
    uint objectIds[];
};

layout(binding = 11) readonly buffer ModelMatrixBuffer {
    mat4 modelMatrices[];
};

//=============================================================================
// Main
//=============================================================================

void main()
{
    // Pass through to geometry shader (transformation happens there)
    outPosition        = inPosition;
    outNormal          = inNormal;
    outTexCoord        = inTexCoord;
    outTangent         = inTangent;
    outClusterInfo     = inClusterInfo;
    outClusterGroupInfo = inClusterGroupInfo;
}
