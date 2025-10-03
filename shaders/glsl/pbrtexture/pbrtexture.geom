#version 450

/*
 * Forward PBR Geometry Shader
 * Transforms vertices using per-object model matrices
 */
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

//=============================================================================
// Geometry Inputs (from Vertex Shader)
//=============================================================================
layout(location = 0) in vec3 inPosition[3];
layout(location = 1) in vec3 inNormal[3];
layout(location = 2) in vec2 inTexCoord[3];
layout(location = 3) in vec4 inTangent[3];
layout(location = 4) in vec4 inClusterInfo[3];
layout(location = 5) in vec4 inClusterGroupInfo[3];

//=============================================================================
// Geometry Outputs (to Fragment Shader)
//=============================================================================
layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out vec4 outTangent;
layout(location = 4) out vec4 outClusterInfo;
layout(location = 5) out vec4 outClusterGroupInfo;
layout(location = 6) out flat uint outObjectId;

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
    for (uint i = 0; i < 3; i++)
    {
        // Get object ID for this vertex
        uint objectId = objectIds[gl_PrimitiveIDIn * 3 + i];
        mat4 modelMatrix = modelMatrices[objectId];
        mat3 normalMatrix = mat3(modelMatrix);  // Assumes uniform scale
        
        // Transform position to world space
        outWorldPos = (modelMatrix * vec4(inPosition[i], 1.0)).xyz;
        
        // Transform normal and tangent
        outNormal  = normalMatrix * inNormal[i];
        outTangent = vec4(normalMatrix * inTangent[i].xyz, inTangent[i].w);
        
        // Pass through other attributes
        outTexCoord        = inTexCoord[i];
        outClusterInfo     = inClusterInfo[i];
        outClusterGroupInfo = inClusterGroupInfo[i];
        outObjectId        = objectId;
        
        // Transform to clip space
        gl_Position = camera.projection * camera.view * vec4(outWorldPos, 1.0);
        
        EmitVertex();
    }
    
    EndPrimitive();
}
