/*
 * Forward PBR Fragment Shader
 * Standard PBR shading with IBL support for forward rendering path
 */
#version 450

#include "pbr_common.glsl"

//=============================================================================
// Shader I/O
//=============================================================================
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec4 inClusterInfo;
layout(location = 5) in vec4 inClusterGroupInfo;
layout(location = 6) in flat uint inObjectId;

layout(location = 0) out vec4 outColor;

//=============================================================================
// Uniform Buffers
//=============================================================================
layout(binding = 0) uniform CameraUBO {
    mat4 projection;
    mat4 model;
    mat4 view;
    vec3 cameraPos;
} camera;

layout(binding = 1) uniform LightingUBO {
    vec4  lightPositions[4];
    float exposure;
    float gamma;
} lighting;

//=============================================================================
// IBL Textures
//=============================================================================
layout(binding = 2) uniform samplerCube irradianceMap;
layout(binding = 3) uniform sampler2D   brdfLUT;
layout(binding = 4) uniform samplerCube prefilterMap;

//=============================================================================
// Material Textures
//=============================================================================
layout(binding = 5) uniform sampler2D albedoMap;
layout(binding = 6) uniform sampler2D normalMap;
layout(binding = 7) uniform sampler2D aoMap;
layout(binding = 8) uniform sampler2D metallicMap;
layout(binding = 9) uniform sampler2D roughnessMap;

//=============================================================================
// Push Constants
//=============================================================================
layout(push_constant) uniform PushConstants {
    int visualizationMode;  // 0: PBR, 1: LOD, 2: Cluster/ClusterGroup
} pushConsts;

//=============================================================================
// Constants
//=============================================================================
#define DEFAULT_ALBEDO vec3(0.5)

//=============================================================================
// IBL Functions
//=============================================================================

vec3 samplePrefilterMap(vec3 R, float roughness)
{
    float lod = roughness * MAX_REFLECTION_LOD;
    float lodFloor = floor(lod);
    float lodCeil  = ceil(lod);
    vec3 a = textureLod(prefilterMap, R, lodFloor).rgb;
    vec3 b = textureLod(prefilterMap, R, lodCeil).rgb;
    return mix(a, b, lod - lodFloor);
}

//=============================================================================
// Normal Mapping
//=============================================================================

vec3 calculateTBNNormal()
{
    vec3 tangentNormal = texture(normalMap, inTexCoord).xyz * 2.0 - 1.0;
    
    vec3 N = normalize(inNormal);
    vec3 T = normalize(inTangent.xyz);
    vec3 B = normalize(cross(N, T) * inTangent.w);
    mat3 TBN = mat3(T, B, N);
    
    return normalize(TBN * tangentNormal);
}

//=============================================================================
// PBR Lighting
//=============================================================================

vec3 calcDirectLighting(vec3 L, vec3 V, vec3 N, vec3 F0,
                         float metallic, float roughness, vec3 albedo)
{
    vec3 H = normalize(V + L);
    
    float NdotH = max(dot(N, H), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    
    if (NdotL <= 0.0) return vec3(0.0);
    
    // Cook-Torrance BRDF
    float D = distributionGGX(NdotH, roughness);
    float G = geometrySmith(NdotL, NdotV, roughness);
    vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 specular = (D * G * F) / (4.0 * NdotL * NdotV + EPSILON);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    
    return (kD * albedo * INV_PI + specular) * NdotL;
}

vec3 calcIndirectLighting(vec3 V, vec3 N, vec3 R, vec3 F0,
                           float metallic, float roughness, vec3 albedo)
{
    float NdotV = max(dot(N, V), 0.0);
    
    vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    
    // Diffuse IBL
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuse = irradiance * albedo;
    
    // Specular IBL
    vec3 prefilteredColor = samplePrefilterMap(R, roughness);
    vec2 envBRDF = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);
    
    return kD * diffuse + specular;
}

//=============================================================================
// Visualization
//=============================================================================

vec4 visualizeCluster()
{
    // Toggle between cluster and cluster group based on gamma threshold
    if (lighting.gamma < 2.15) {
        return vec4(inClusterGroupInfo.rgb, 1.0);
    }
    return vec4(inClusterInfo.rgb, 1.0);
}

vec4 visualizeLodLevel()
{
    return vec4(lodLevelToColor(inClusterInfo.x), 1.0);
}

//=============================================================================
// Main
//=============================================================================

void main()
{
    // Handle visualization modes
    if (pushConsts.visualizationMode == 2) {
        outColor = visualizeCluster();
        return;
    }
    else if (pushConsts.visualizationMode == 1) {
        outColor = visualizeLodLevel();
        return;
    }
    
    // Normal (use vertex normal for now, enable normal mapping if needed)
    vec3 N = normalize(inNormal);
    // vec3 N = calculateTBNNormal();  // Enable for normal mapping
    
    vec3 V = normalize(camera.cameraPos - inWorldPos);
    vec3 R = reflect(-V, N);
    
    // Material properties (hardcoded for now, TODO: sample from textures)
    vec3  albedo    = DEFAULT_ALBEDO;
    float metallic  = 0.1;
    float roughness = 0.8;
    // vec3  albedo    = pow(texture(albedoMap, inTexCoord).rgb, vec3(2.2));
    // float metallic  = texture(metallicMap, inTexCoord).r;
    // float roughness = texture(roughnessMap, inTexCoord).r;
    
    // Calculate F0
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    
    // Direct lighting
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < 4; i++) {
        vec3 L = normalize(lighting.lightPositions[i].xyz - inWorldPos);
        Lo += calcDirectLighting(L, V, N, F0, metallic, roughness, albedo);
    }
    
    // Indirect lighting (IBL)
    vec3 ambient = calcIndirectLighting(V, N, R, F0, metallic, roughness, albedo);
    
    // Final color
    vec3 color = ambient + Lo;
    
    // Tone mapping and gamma correction
    color = applyToneMapping(color, lighting.exposure, lighting.gamma);
    
    outColor = vec4(color, 1.0);
}
