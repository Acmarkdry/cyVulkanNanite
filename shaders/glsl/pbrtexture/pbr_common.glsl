/*
 * PBR Common Functions
 * Shared PBR lighting utilities for Nanite-style rendering pipeline
 */

#ifndef PBR_COMMON_GLSL
#define PBR_COMMON_GLSL

//=============================================================================
// Constants
//=============================================================================
#define PI              3.14159265359
#define TWO_PI          6.28318530718
#define INV_PI          0.31830988618
#define EPSILON         0.0001

#define MAX_REFLECTION_LOD  9.0
#define INVALID_ID          0xFFFFFFFF

//=============================================================================
// Tone Mapping
//=============================================================================

// Uncharted 2 filmic tone mapping
// Reference: http://filmicgames.com/archives/75
vec3 tonemapUncharted2(vec3 x)
{
    const float A = 0.15;  // Shoulder Strength
    const float B = 0.50;  // Linear Strength
    const float C = 0.10;  // Linear Angle
    const float D = 0.20;  // Toe Strength
    const float E = 0.02;  // Toe Numerator
    const float F = 0.30;  // Toe Denominator
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

// Apply full tone mapping with exposure and gamma correction
vec3 applyToneMapping(vec3 color, float exposure, float gamma)
{
    // Exposure
    color = tonemapUncharted2(color * exposure);
    color = color * (1.0 / tonemapUncharted2(vec3(11.2)));
    // Gamma correction
    return pow(color, vec3(1.0 / gamma));
}

//=============================================================================
// PBR Functions - Normal Distribution
//=============================================================================

// GGX/Trowbridge-Reitz normal distribution function
float distributionGGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

//=============================================================================
// PBR Functions - Geometry
//=============================================================================

// Schlick-GGX geometry function (single direction)
float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

// Smith's geometry function (combined)
float geometrySmith(float NdotL, float NdotV, float roughness)
{
    return geometrySchlickGGX(NdotL, roughness) * geometrySchlickGGX(NdotV, roughness);
}

//=============================================================================
// PBR Functions - Fresnel
//=============================================================================

// Schlick's Fresnel approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Schlick's Fresnel with roughness (for IBL)
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

//=============================================================================
// Utility Functions
//=============================================================================

// Hash uint to vec4 for debug visualization
vec4 hashToColor(uint value)
{
    const uint PRIME1 = 2654435761u;
    const uint PRIME2 = 2246822519u;
    const uint PRIME3 = 3266489917u;
    const uint PRIME4 = 668265263u;

    uvec4 h = uvec4(value) * uvec4(PRIME1, PRIME2, PRIME3, PRIME4);
    
    const float scale = 4.0 / 255.0;
    return vec4(
        exp(-scale * float(255u - (h.x & 0xFFu))),
        exp(-scale * float(255u - (h.y & 0xFFu))),
        exp(-scale * float(255u - (h.z & 0xFFu))),
        exp(-scale * float(255u - (h.w & 0xFFu)))
    );
}

// LOD level to color for visualization
vec3 lodLevelToColor(float level)
{
    const vec3 colors[6] = vec3[6](
        vec3(1.0, 0.0, 0.0),  // Level 0 - Red
        vec3(0.0, 1.0, 0.0),  // Level 1 - Green
        vec3(0.0, 0.0, 1.0),  // Level 2 - Blue
        vec3(1.0, 1.0, 0.0),  // Level 3 - Yellow
        vec3(1.0, 0.0, 1.0),  // Level 4 - Magenta
        vec3(0.0, 1.0, 1.0)   // Level 5 - Cyan
    );
    int idx = clamp(int(level), 0, 5);
    return colors[idx];
}

// Calculate barycentric coordinates for a point in a triangle
vec3 calcBarycentric(vec3 p, vec3 v0, vec3 v1, vec3 v2)
{
    vec3 e0 = v1 - v0;
    vec3 e1 = v2 - v0;
    float invArea = 1.0 / length(cross(e0, e1));

    vec3 bary;
    bary.x = length(cross(v2 - v1, p - v1)) * invArea;
    bary.y = length(cross(v0 - v2, p - v2)) * invArea;
    bary.z = 1.0 - bary.x - bary.y;

    return bary;
}

#endif // PBR_COMMON_GLSL
