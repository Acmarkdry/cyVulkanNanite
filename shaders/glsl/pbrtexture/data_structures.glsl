/*
 * Common Data Structures
 * Shared data structures for Nanite-style rendering pipeline
 */

#ifndef DATA_STRUCTURES_GLSL
#define DATA_STRUCTURES_GLSL

//=============================================================================
// Cluster Data
//=============================================================================
struct Cluster
{
    vec3  boundMin;       // AABB minimum
    vec3  boundMax;       // AABB maximum
    uint  triangleStart;  // Start index in triangle buffer
    uint  triangleEnd;    // End index in triangle buffer
    uint  objectId;       // Parent object ID
};

//=============================================================================
// Vertex Data
//=============================================================================
struct Vertex
{
    vec3 position;
    vec3 normal;
    vec2 texCoord;
    vec4 color;
    vec4 clusterInfo;     // x: LOD level, w: cluster ID
    vec4 weights;         // Bone weights (for skinning)
    vec4 tangent;         // xyz: tangent, w: bitangent sign
};

//=============================================================================
// Visibility Buffer ID Encoding/Decoding
//=============================================================================

// Pack cluster ID and triangle ID into a single uint
// Format: [clusterID (24 bits)][triangleID (8 bits)]
uint packVisibilityId(uint clusterId, uint triangleId)
{
    return (clusterId << 8u) | (triangleId & 0xFFu);
}

// Unpack cluster ID from visibility ID
uint unpackClusterId(uint visId)
{
    return visId >> 8u;
}

// Unpack triangle ID from visibility ID
uint unpackTriangleId(uint visId)
{
    return visId & 0xFFu;
}

#endif // DATA_STRUCTURES_GLSL
