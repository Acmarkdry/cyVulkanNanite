#version 450
#extension GL_GOOGLE_include_directive:enable
#include "pbr_common.glsl"
#include "data_structures.glsl"

//=============================================================================
// Descriptor Set 0 - Geometry Data
//=============================================================================
layout(std430, set = 0, binding = 0) readonly buffer ClusterBuffer {
    Cluster clusters[];
};

layout(std430, set = 0, binding = 1) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(std430, set = 0, binding = 2) readonly buffer IndexBuffer {
    uint indices[];
};

layout(std430, set = 0, binding = 3) readonly buffer ModelMatrixBuffer {
    mat4 modelMatrices[];
};

//=============================================================================
// Descriptor Set 0 - Visibility Buffer
//=============================================================================
layout(set = 0, binding = 4, r32ui) uniform readonly uimage2D visibilityBuffer;
layout(set = 0, binding = 5, r32f)  uniform readonly image2D  depthBuffer;

//=============================================================================
// Descriptor Set 0 - Camera & Lighting
//=============================================================================
layout(set = 0, binding = 6) uniform CameraUBO {
    mat4 invView;
    mat4 invProj;
    vec3 cameraPos;
} camera;

layout(binding = 7) uniform LightingUBO {
    vec4  lightPositions[4];
    float exposure;
    float gamma;
} lighting;

//=============================================================================
// Descriptor Set 0 - IBL Textures
//=============================================================================
layout(binding = 8)  uniform samplerCube irradianceMap;
layout(binding = 9)  uniform sampler2D   brdfLUT;
layout(binding = 10) uniform samplerCube prefilterMap;

//=============================================================================
// Push Constants
//=============================================================================
layout(push_constant) uniform PushConstants {
    int visualizationMode;  // 0: PBR, 1: LOD, 2: ClusterID, 3: TriangleID
} pushConsts;

//=============================================================================
// Shader I/O
//=============================================================================
layout(location = 0) in  vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

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
    
    // Fresnel
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
// Visualization Modes
//=============================================================================

void visualizeClusterId(uint visId)
{
    vec3 color = hashToColor(visId).rgb;
    outColor = vec4(applyToneMapping(color, lighting.exposure, lighting.gamma), 1.0);
}

void visualizeLodLevel(float lodLevel)
{
    vec3 color = lodLevelToColor(lodLevel);
    outColor = vec4(color, 1.0);
}

void visualizeTriangleId(uint visId)
{
    float t = float(visId) * 0.5 + 0.25;
    outColor = vec4(vec3(t), 1.0);
}

//=============================================================================
// Main
//=============================================================================

void main()
{
    ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
    
    // Load visibility data
    uint visId = imageLoad(visibilityBuffer, pixelCoord).x;
    if (visId == INVALID_ID) discard;
    
    float depth = imageLoad(depthBuffer, pixelCoord).x;
    
    // Decode visibility ID
    uint clusterId  = unpackClusterId(visId);
    uint triangleId = unpackTriangleId(visId);
    
    // Get cluster and triangle data
    Cluster cluster = clusters[clusterId];
    uint globalTriId = cluster.triangleStart + triangleId;
    
    uint i0 = indices[globalTriId * 3 + 0];
    uint i1 = indices[globalTriId * 3 + 1];
    uint i2 = indices[globalTriId * 3 + 2];
    
    // Handle visualization modes
    if (pushConsts.visualizationMode == 2) {
        visualizeClusterId(visId);
        return;
    }
    else if (pushConsts.visualizationMode == 1) {
        float lodLevel = vertices[i0].clusterInfo.x;
        visualizeLodLevel(lodLevel);
        return;
    }
    else if (pushConsts.visualizationMode == 3) {
        visualizeTriangleId(visId);
        return;
    }
    
    // Reconstruct world position from depth
    vec4 ndc = vec4(inTexCoord * 2.0 - 1.0, depth, 1.0);
    vec4 worldPos = camera.invView * camera.invProj * ndc;
    worldPos.xyz /= worldPos.w;
    
    // Get model matrix and transform vertices
    mat4 model = modelMatrices[cluster.objectId];
    vec3 v0World = (model * vec4(vertices[i0].position, 1.0)).xyz;
    vec3 v1World = (model * vec4(vertices[i1].position, 1.0)).xyz;
    vec3 v2World = (model * vec4(vertices[i2].position, 1.0)).xyz;
    
    // Calculate barycentric coordinates
    vec3 bary = calcBarycentric(worldPos.xyz, v0World, v1World, v2World);
    
    // Interpolate normal (using normal matrix for correct transformation)
    mat3 normalMatrix = mat3(model);  // Assumes uniform scale
    vec3 n0 = normalMatrix * vertices[i0].normal;
    vec3 n1 = normalMatrix * vertices[i1].normal;
    vec3 n2 = normalMatrix * vertices[i2].normal;
    vec3 N = normalize(n0 * bary.x + n1 * bary.y + n2 * bary.z);
    
    // Interpolate texture coordinates (for future texture sampling)
    vec2 uv = vertices[i0].texCoord * bary.x + 
              vertices[i1].texCoord * bary.y + 
              vertices[i2].texCoord * bary.z;
    
    // Interpolate vertex color
    vec3 vertexColor = vertices[i0].color.rgb * bary.x +
                       vertices[i1].color.rgb * bary.y +
                       vertices[i2].color.rgb * bary.z;
    
    // Material properties (TODO: sample from textures)
    vec3  albedo    = mix(vec3(0.5), vertexColor, step(0.01, length(vertexColor)));
    float metallic  = 0.1;
    float roughness = 0.8;
    
    // Calculate view and reflection vectors
    vec3 V = normalize(camera.cameraPos - worldPos.xyz);
    vec3 R = reflect(-V, N);
    
    // Calculate F0 (base reflectivity)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    
    // Accumulate direct lighting
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < 4; i++) {
        vec3 lightPos = lighting.lightPositions[i].xyz;
        vec3 L = normalize(lightPos - worldPos.xyz);
        Lo += calcDirectLighting(L, V, N, F0, metallic, roughness, albedo);
    }
    
    // Add indirect lighting (IBL)
    vec3 ambient = calcIndirectLighting(V, N, R, F0, metallic, roughness, albedo);
    
    // Final color
    vec3 color = ambient + Lo;
    
    // Tone mapping and gamma correction
    color = applyToneMapping(color, lighting.exposure, lighting.gamma);
    
    outColor = vec4(color, 1.0);
}
