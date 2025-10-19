/*
 * common_culling.glsl
 * 公共剔除函数库 — Nanite 风格渲染管线
 *
 * 提取自 bvhtraversal.comp / culling.comp / error.comp 中的重复代码
 * 所有函数通过参数接收矩阵，不依赖特定的 uniform 名称
 */

#ifndef COMMON_CULLING_GLSL
#define COMMON_CULLING_GLSL

//=============================================================================
// AABB 工具
//=============================================================================

// 由 3-bit 索引 [0..7] 从 pMin/pMax 生成 AABB 的 8 个角点
// bit0 → x, bit1 → y, bit2 → z  (0 取 pMin, 1 取 pMax)
vec4 getAABBCorner(vec3 pMin, vec3 pMax, uint idx)
{
    return vec4(
        ((idx & 1u) != 0u) ? pMax.x : pMin.x,
        ((idx & 2u) != 0u) ? pMax.y : pMin.y,
        ((idx & 4u) != 0u) ? pMax.z : pMin.z,
        1.0
    );
}

//=============================================================================
// 屏幕空间 AABB 计算
//=============================================================================

// 将世界空间 AABB 投影到屏幕空间，输出归一化 [0,1] 范围的 screenXY 和最小深度
// viewProj: 用于投影的 VP 矩阵（通常是上一帧的 proj * view）
void getScreenAABB(vec3 pMin, vec3 pMax, mat4 viewProj,
                   inout vec4 screenXY, inout float minZ)
{
    vec2 minXY = vec2(1.0);
    vec2 maxXY = vec2(0.0);
    minZ = 1.0;

    for (uint i = 0u; i < 8u; i++)
    {
        vec4 corner = getAABBCorner(pMin, pMax, i);
        vec4 h = viewProj * corner;
        h.xyz /= h.w;
        h.xy = h.xy * 0.5 + 0.5;

        minZ  = min(minZ, h.z);
        minXY = min(minXY, h.xy);
        maxXY = max(maxXY, h.xy);
    }

    screenXY = vec4(minXY, maxXY);
}

//=============================================================================
// 视锥体剔除
//=============================================================================

// 如果 AABB 完全在视锥体外则返回 true（应被剔除）
// viewProj: 当前帧的 proj * view
bool frustumCulling(vec3 pMin, vec3 pMax, mat4 viewProj)
{
    const float eps = 1e-3;
    bool inFrustum = false;

    for (uint i = 0u; i < 8u; i++)
    {
        vec4 corner = getAABBCorner(pMin, pMax, i);
        vec4 hpos = viewProj * corner;
        if (hpos.w == 0.0) return false;
        hpos.xyz /= hpos.w;
        inFrustum = inFrustum ||
            (hpos.x > -1.0 - eps && hpos.x < 1.0 + eps &&
             hpos.y > -1.0 - eps && hpos.y < 1.0 + eps &&
             hpos.z >  0.0 - eps && hpos.z < 1.0 + eps);
    }

    return !inFrustum;
}

//=============================================================================
// 遮挡剔除 (HZB)
//=============================================================================

// 基于 Hierarchical Z-Buffer 的遮挡剔除
// 如果被完全遮挡则返回 true（应被剔除）
// lastViewProj: 上一帧 VP 矩阵，hzbSampler: 上一帧 HZB 纹理
// pixelArea: 输出屏幕空间像素面积（用于 SW/HW 光栅化分流）
bool occlusionCulling(vec3 pMin, vec3 pMax, mat4 lastViewProj,
                      sampler2D hzbSampler, out float pixelArea)
{
    vec4 clipXY;
    float minZ;
    getScreenAABB(pMin, pMax, lastViewProj, clipXY, minZ);

    vec4 screenSize = textureSize(hzbSampler, 0).xyxy;
    vec4 screenXY   = clipXY * screenSize;
    vec2 screenSpan = screenXY.zw - screenXY.xy;
    pixelArea = screenSpan.x * screenSpan.y;

    float hzbLevel   = ceil(log2(max(screenSpan.x, screenSpan.y)));
    float hzbLevel_1 = max(hzbLevel - 1.0, 0.0);
    float texScale   = exp2(-hzbLevel_1);
    vec2  texSpan    = ceil(screenXY.zw * texScale) - floor(screenXY.xy * texScale);
    if (texSpan.x < 2.0 && texSpan.y < 2.0) hzbLevel = hzbLevel_1;

    float z1 = textureLod(hzbSampler, vec2(clipXY.x, clipXY.y), hzbLevel).x;
    float z2 = textureLod(hzbSampler, vec2(clipXY.x, clipXY.w), hzbLevel).x;
    float z3 = textureLod(hzbSampler, vec2(clipXY.z, clipXY.y), hzbLevel).x;
    float z4 = textureLod(hzbSampler, vec2(clipXY.z, clipXY.w), hzbLevel).x;
    float maxHiz = max(max(z1, z2), max(z3, z4));

    return minZ > maxHiz;
}

// 简化版遮挡剔除（不需要 pixelArea 输出）
bool occlusionCulling(vec3 pMin, vec3 pMax, mat4 lastViewProj,
                      sampler2D hzbSampler)
{
    float unused;
    return occlusionCulling(pMin, pMax, lastViewProj, hzbSampler, unused);
}

//=============================================================================
// 屏幕空间误差估计
//=============================================================================

// 计算球体在屏幕上的投影半径平方（像素单位）
// 用于 LOD 误差判定
// viewProj: VP 矩阵, camUp/camRight: 相机方向, screenSize: 屏幕分辨率
float getScreenBoundRadiusSq(vec3 center, float R,
                             mat4 viewProj,
                             vec3 camUp, vec3 camRight,
                             vec2 screenSize)
{
    vec4 c = viewProj * vec4(center, 1.0);
    c.xy /= c.w;
    c.xy = c.xy * 0.5 + 0.5;

    vec4 p0 = viewProj * vec4(R * camUp + center, 1.0);
    p0.xy /= p0.w;
    p0.xy = p0.xy * 0.5 + 0.5;

    vec4 p1 = viewProj * vec4(R * camRight + center, 1.0);
    p1.xy /= p1.w;
    p1.xy = p1.xy * 0.5 + 0.5;

    vec2 v0 = (p0.xy - c.xy) * screenSize;
    vec2 v1 = (p1.xy - c.xy) * screenSize;

    return max(dot(v0, v0), dot(v1, v1));
}

#endif // COMMON_CULLING_GLSL
