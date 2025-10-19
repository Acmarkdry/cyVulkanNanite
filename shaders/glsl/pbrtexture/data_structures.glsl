/*
 * data_structures.glsl
 * Common Data Structures — Nanite-style rendering pipeline
 *
 * 所有着色器共享的数据结构定义
 */

#ifndef DATA_STRUCTURES_GLSL
#define DATA_STRUCTURES_GLSL

//=============================================================================
// Cluster 数据
//=============================================================================
struct Cluster
{
    vec3  pMin;           // AABB minimum
    vec3  pMax;           // AABB maximum
    uint  triangleStart;  // Start index in triangle buffer
    uint  triangleEnd;    // End index in triangle buffer
    uint  objectId;       // Parent object ID
};

//=============================================================================
// BVH 节点数据 (用于 BVH 遍历)
//=============================================================================
struct BVHNodeInfo
{
    uint  start;                  // 叶节点: cluster 范围起始
    uint  end;                    // 叶节点: cluster 范围结束 (start==end 表示非叶节点)
    vec3  pMin;                   // AABB minimum
    vec3  pMax;                   // AABB maximum
    uint  objectId;               // 所属对象 ID
    vec4  errorR;                 // xyz: 误差球心, w: 误差半径
    vec4  errorRP;                // xyz: 父误差球心, w: 父误差半径
    vec2  errorWorld;             // x: 节点误差, y: 父节点误差 (世界空间)
    ivec4 childrenNodeIndices;    // 子节点索引 (-1 表示无效)
};

//=============================================================================
// LOD 误差数据 (用于逐 cluster 误差计算)
//=============================================================================
struct ErrorInfo
{
    vec2 errorWorld;   // x: 节点误差, y: 父节点误差 (世界空间)
    vec4 centerR;      // xyz: 误差球心, w: 半径
    vec4 centerRP;     // xyz: 父误差球心, w: 父半径
};

//=============================================================================
// 顶点数据
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
// 可见性 ID 编码/解码
// 格式: [clusterID (15 bits)][objectID (11 bits)][triangleID (6 bits)]
//=============================================================================

uint packVisibilityId(uint clusterId, uint objectId, uint triangleId)
{
    return ((clusterId & 0x7FFFu) << 17u) | ((objectId & 0x7FFu) << 6u) | (triangleId & 0x3Fu);
}

uint unpackClusterId(uint visId)
{
    return visId >> 17u;
}

uint unpackObjectId(uint visId)
{
    return (visId >> 6u) & 0x7FFu;
}

uint unpackTriangleId(uint visId)
{
    return visId & 0x3Fu;
}

#endif // DATA_STRUCTURES_GLSL
