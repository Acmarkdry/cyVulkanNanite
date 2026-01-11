// culling.hlsl - Compute Shader for Cluster Culling
// Shader Model 6.0+

#define WORKGROUP_SIZE 64
#define AABB_VERTEX_COUNT 8
#define FRUSTUM_EPS 1e-3f
#define SW_RASTER_THRESHOLD 256.0f


struct Cluster
{
    float3 pMin;
    float3 pMax;
    uint triangleStart;
    uint triangleEnd;
    uint objectId;
};

struct IndirectDrawArgs
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint vertexOffset;
    uint firstInstance;
};

struct IndirectDrawArgs_SW
{
    uint indexCount;
};


cbuffer UBOMats : register(b0)
{
    float4x4 model;      // unused
    float4x4 lastView;
    float4x4 lastProj;
    float4x4 currView;
    float4x4 currProj;
};

cbuffer PushConstants : register(b1)
{
    int numClusters;
    float threshold;
    int useFrustrumOcclusion;
    int useSoftwareRast;
};


StructuredBuffer<Cluster>       indata          : register(t0);
StructuredBuffer<uint>          inTriangles     : register(t1);
StructuredBuffer<float2>        errorData       : register(t2);
Texture2D<float>                lastHZB         : register(t3);
SamplerState                    hzbSampler      : register(s0);

RWStructuredBuffer<uint>        outTriangles_hw : register(u0);
RWStructuredBuffer<uint>        outTriangles_sw : register(u1);
RWStructuredBuffer<uint3>       outIds_hw       : register(u2);
RWStructuredBuffer<uint3>       outIds_sw       : register(u3);
RWStructuredBuffer<IndirectDrawArgs>    numVertices_hw : register(u4);
RWStructuredBuffer<IndirectDrawArgs_SW> numVertices_sw : register(u5);


// AABB的8个角点
void GetAABBVertices(in Cluster c, out float4 vertices[AABB_VERTEX_COUNT])
{
    float3 pMin = c.pMin;
    float3 pMax = c.pMax;
    
    vertices[0] = float4(pMin.x, pMin.y, pMin.z, 1.0f);
    vertices[1] = float4(pMax.x, pMin.y, pMin.z, 1.0f);
    vertices[2] = float4(pMin.x, pMax.y, pMin.z, 1.0f);
    vertices[3] = float4(pMin.x, pMin.y, pMax.z, 1.0f);
    vertices[4] = float4(pMax.x, pMax.y, pMin.z, 1.0f);
    vertices[5] = float4(pMin.x, pMax.y, pMax.z, 1.0f);
    vertices[6] = float4(pMax.x, pMin.y, pMax.z, 1.0f);
    vertices[7] = float4(pMax.x, pMax.y, pMax.z, 1.0f);
}

float3 TransformToNDC(in float4 worldPos, in float4x4 viewProj)
{
    float4 clipPos = mul(viewProj, worldPos);
    return clipPos.xyz / clipPos.w;
}

// vulkan坐标系问题 [0,1]
float2 NDCToUV(in float2 ndc)
{
    return ndc * 0.5f + 0.5f;
}

bool IsInFrustum(in float3 ndc)
{
    return ndc.x > -1.0f - FRUSTUM_EPS && ndc.x < 1.0f + FRUSTUM_EPS &&
           ndc.y > -1.0f - FRUSTUM_EPS && ndc.y < 1.0f + FRUSTUM_EPS &&
           ndc.z > 0.0f - FRUSTUM_EPS  && ndc.z < 1.0f + FRUSTUM_EPS;
}


void GetScreenAABB(in Cluster c, in float4x4 viewProj, out float4 screenXY, out float minZ)
{
    float4 vertices[AABB_VERTEX_COUNT];
    GetAABBVertices(c, vertices);
    
    float2 minXY = float2(1.0f, 1.0f);
    float2 maxXY = float2(0.0f, 0.0f);
    minZ = 1.0f;
    
    // 编译器自己展开
    [unroll]
    for (int i = 0; i < AABB_VERTEX_COUNT; ++i)
    {
        float3 ndc = TransformToNDC(vertices[i], viewProj);
        float2 uv = NDCToUV(ndc.xy);
        
        minXY = min(minXY, uv);
        maxXY = max(maxXY, uv);
        minZ = min(minZ, ndc.z);
    }
    
    screenXY = float4(minXY, maxXY);
}


bool FrustumCulling(in Cluster c, in float4x4 viewProj)
{
    float4 vertices[AABB_VERTEX_COUNT];
    GetAABBVertices(c, vertices);
    
    [unroll]
    for (int i = 0; i < AABB_VERTEX_COUNT; ++i)
    {
        float4 clipPos = mul(viewProj, vertices[i]);
        
        // 避免除零
        if (clipPos.w == 0.0f)
            return true;
        
        float3 ndc = clipPos.xyz / clipPos.w;
        
        // 任意一个顶点在视锥体内则不剔除
        if (IsInFrustum(ndc))
            return false;
    }
    
    return true;
}


bool OcclusionCulling(in Cluster c, in float4x4 viewProj, out float pixelArea)
{
    float4 clipXY;
    float minZ;
    GetScreenAABB(c, viewProj, clipXY, minZ);
    
    // 获取HZB尺寸
    uint width, height, mipLevels;
    lastHZB.GetDimensions(0, width, height, mipLevels);
    float2 screenSize = float2(width, height);
    
    // 计算屏幕空间范围
    float4 screenXY = clipXY * screenSize.xyxy;
    float2 screenSpan = screenXY.zw - screenXY.xy;
    pixelArea = screenSpan.x * screenSpan.y;
    
    // 计算合适的HiZ mip级别
    float maxSpan = max(screenSpan.x, screenSpan.y);
    float hzbLevel = ceil(log2(maxSpan));
    float hzbLevel_1 = max(hzbLevel - 1.0f, 0.0f);
    
    // 检查是否可以使用更低的mip级别
    float texScale = exp2(-hzbLevel_1);
    float2 texSpan = ceil(screenXY.zw * texScale) - floor(screenXY.xy * texScale);
    if (texSpan.x < 2.0f && texSpan.y < 2.0f)
    {
        hzbLevel = hzbLevel_1;
    }
    
    // 采样HiZ四个角点
    float z1 = lastHZB.SampleLevel(hzbSampler, clipXY.xy, hzbLevel);
    float z2 = lastHZB.SampleLevel(hzbSampler, clipXY.xw, hzbLevel);
    float z3 = lastHZB.SampleLevel(hzbSampler, clipXY.zy, hzbLevel);
    float z4 = lastHZB.SampleLevel(hzbSampler, clipXY.zw, hzbLevel);
    
    float maxHiz = max(max(z1, z2), max(z3, z4));
    
    return minZ > maxHiz;
}


bool ErrorCulling(uint clusterIndex, float errorThreshold)
{
    float2 error = errorData[clusterIndex];
    // 当前LOD误差小于阈值 或 父LOD误差大于阈值时剔除
    return error.y <= errorThreshold || error.x > errorThreshold;
}


void OutputTriangles_HW(in Cluster cluster, uint clusterIdx, uint startIdx, uint triCount)
{
    uint baseIn = cluster.triangleStart * 3;
    
    for (uint i = 0; i < triCount; ++i)
    {
        uint inIdx = baseIn + i * 3;
        uint outIdx = startIdx + i * 3;
        
        outTriangles_hw[outIdx + 0] = inTriangles[inIdx + 0];
        outTriangles_hw[outIdx + 1] = inTriangles[inIdx + 1];
        outTriangles_hw[outIdx + 2] = inTriangles[inIdx + 2];
        outIds_hw[outIdx / 3] = uint3(cluster.objectId, clusterIdx, i);
    }
}

void OutputTriangles_SW(in Cluster cluster, uint clusterIdx, uint startIdx, uint triCount)
{
    uint baseIn = cluster.triangleStart * 3;
    
    for (uint i = 0; i < triCount; ++i)
    {
        uint inIdx = baseIn + i * 3;
        uint outIdx = startIdx + i * 3;
        
        outTriangles_sw[outIdx + 0] = inTriangles[inIdx + 0];
        outTriangles_sw[outIdx + 1] = inTriangles[inIdx + 1];
        outTriangles_sw[outIdx + 2] = inTriangles[inIdx + 2];
        outIds_sw[outIdx / 3] = uint3(cluster.objectId, clusterIdx, i);
    }
}


[numthreads(WORKGROUP_SIZE, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.x;
    
    if (index >= (uint)numClusters)
        return;
    
    Cluster currCluster = indata[index];
    
    float4x4 lastViewProj = mul(lastProj, lastView);
    float4x4 currViewProj = mul(currProj, currView);
    
    bool culled = false;
    float pixelArea = 0.0f;
    
    if (useFrustrumOcclusion == 1)
    {
        culled = FrustumCulling(currCluster, currViewProj);
        
        if (!culled)
        {
            culled = OcclusionCulling(currCluster, lastViewProj, pixelArea);
        }
    }
    else
    {
        // 软光栅
        float4 clipXY;
        float minZ;
        GetScreenAABB(currCluster, lastViewProj, clipXY, minZ);
        
        uint width, height, mipLevels;
        // 拿到纹理缓冲区信息
        lastHZB.GetDimensions(0, width, height, mipLevels);
        float2 screenSize = float2(width, height);
        
        float2 screenSpan = (clipXY.zw - clipXY.xy) * screenSize;
        pixelArea = screenSpan.x * screenSpan.y;
    }
    
    culled = culled || ErrorCulling(index, threshold);
    
    bool useSWR = (useSoftwareRast == 1) && (pixelArea < SW_RASTER_THRESHOLD);
    
    uint triangleCount = currCluster.triangleEnd - currCluster.triangleStart;
    uint totalVertices = culled ? 0 : triangleCount * 3;
    
    uint localIdx;
    // 多线程通信，所以要加锁
    if (!useSWR)
    {
        InterlockedAdd(numVertices_hw[0].indexCount, totalVertices, localIdx);
    }
    else
    {
        InterlockedAdd(numVertices_sw[0].indexCount, totalVertices, localIdx);
    }
    

    if (!culled)
    {
        if (!useSWR)
        {
            OutputTriangles_HW(currCluster, index, localIdx, triangleCount);
        }
        else
        {
            OutputTriangles_SW(currCluster, index, localIdx, triangleCount);
        }
    }
}