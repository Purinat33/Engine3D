#type vertex
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec3 v_NormalWS;
out vec2 v_TexCoord;
out vec3 v_WorldPos;

void main() {
    mat3 normalMat = transpose(inverse(mat3(u_Model)));
    v_NormalWS = normalize(normalMat * a_Normal);
    v_TexCoord = a_TexCoord;

    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    v_WorldPos = worldPos.xyz;

    gl_Position = u_ViewProjection * worldPos;
}

#type fragment
#version 330 core
in vec3 v_NormalWS;
in vec2 v_TexCoord;
in vec3 v_WorldPos;

uniform vec4 u_Color;
uniform int  u_UseTexture;
uniform sampler2D u_Texture0;

uniform int   u_UseLighting;

// Convention used here:
// u_LightDir = direction light RAYS travel in world space (sun direction).
// Vector from surface TO light is -u_LightDir.
uniform vec3  u_LightDir;
uniform vec3  u_LightColor;
uniform float u_Ambient;

uniform int   u_UseShadows;
uniform sampler2DArray u_ShadowMapArray;
uniform int   u_CascadeCount;
uniform float u_CascadeSplits[4];
uniform mat4  u_LightSpaceMatrices[4];
uniform float u_ShadowBias;

uniform mat4 u_View;

// ---------------- DEBUG SWITCH ----------------
// 0 = normal shading
// 1 = shadow factor (white lit, black shadow)
// 2 = cascade index (grayscale)
// 3 = shadow UV visualization
// 4 = shadow failure reason (color-coded)
#define DEBUG_VIEW 0
// ------------------------------------------------

// Reason codes (DEBUG_VIEW==4)
const int REASON_OK              = 0;
const int REASON_TEX_UNBOUND     = 1; // textureSize == 0
const int REASON_BAD_W           = 2; // lp.w <= 0
const int REASON_OUTSIDE_XY      = 3; // proj x/y out of [0..1]
const int REASON_OUTSIDE_Z       = 4; // proj z out of [0..1]
const int REASON_CASCADE_INVALID = 5; // cascadeIdx out of range
const int REASON_SHADOWS_DISABLED = 6;
const int REASON_NO_CASCADES      = 7;

float ShadowFactor(int cascadeIdx, vec3 worldPos, vec3 N, vec3 L, out int reason, out vec3 outProj01)
{
    reason = REASON_OK;
    outProj01 = vec3(0.0);

    if (cascadeIdx < 0 || cascadeIdx >= u_CascadeCount) {
        reason = REASON_CASCADE_INVALID;
        return 0.0;
    }

    // If the shadow texture isn't bound/created correctly, textureSize can be 0.
    ivec3 ts = textureSize(u_ShadowMapArray, 0);
    if (ts.x <= 0 || ts.y <= 0) {
        reason = REASON_TEX_UNBOUND;
        return 0.0;
    }

    mat4 lightMat = u_LightSpaceMatrices[cascadeIdx];
    vec4 lp = lightMat * vec4(worldPos, 1.0);

    if (lp.w <= 0.000001) {
        reason = REASON_BAD_W;
        return 0.0;
    }

    vec3 proj = lp.xyz / lp.w;        // NDC (-1..1)
    vec3 proj01 = proj * 0.5 + 0.5;   // (0..1)
    outProj01 = proj01;

    // outside shadow map => no shadow (lit)
    if (proj01.x < 0.0 || proj01.x > 1.0 || proj01.y < 0.0 || proj01.y > 1.0) {
        reason = REASON_OUTSIDE_XY;
        return 0.0;
    }
    if (proj01.z < 0.0 || proj01.z > 1.0) {
        reason = REASON_OUTSIDE_Z;
        return 0.0;
    }

    float current = proj01.z;

    // slope-scaled bias helps acne
    float bias = max(u_ShadowBias * (1.0 - dot(N, L)), u_ShadowBias);

    vec2 texel = 1.0 / vec2(ts.xy);

    // 3x3 PCF
    float shadow = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float pcfDepth = texture(
                u_ShadowMapArray,
                vec3(proj01.xy + vec2(x, y) * texel, float(cascadeIdx))
            ).r;

            shadow += (current - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    shadow *= (1.0 / 9.0);
    return shadow;
}

out vec4 FragColor;

void main() {
    vec3 albedo = u_Color.rgb;
    if (u_UseTexture == 1)
        albedo *= texture(u_Texture0, v_TexCoord).rgb;

    vec3 N = normalize(v_NormalWS);
    vec3 L = normalize(-u_LightDir);

    float ndl = max(dot(N, L), 0.0);

    // Choose cascade based on view-space depth (positive forward)
    float viewDepth = -(u_View * vec4(v_WorldPos, 1.0)).z;

    int cascadeIdx = 0;
    if (u_CascadeCount > 1 && viewDepth > u_CascadeSplits[0]) cascadeIdx = 1;
    if (u_CascadeCount > 2 && viewDepth > u_CascadeSplits[1]) cascadeIdx = 2;
    if (u_CascadeCount > 3 && viewDepth > u_CascadeSplits[2]) cascadeIdx = 3;
    cascadeIdx = clamp(cascadeIdx, 0, max(u_CascadeCount - 1, 0));

    float shadow = 0.0;
int reason = REASON_OK;
vec3 proj01 = vec3(0.0);

if (u_UseShadows != 1) {
    reason = REASON_SHADOWS_DISABLED;
    shadow = 0.0;
} else if (u_CascadeCount <= 0) {
    reason = REASON_NO_CASCADES;
    shadow = 0.0;
} else {
    shadow = ShadowFactor(cascadeIdx, v_WorldPos, N, L, reason, proj01);
}

#if DEBUG_VIEW == 1
    // Shadow factor visualization (white lit, black shadow)
    FragColor = vec4(vec3(1.0 - shadow), 1.0);
    return;
#elif DEBUG_VIEW == 2
    // Cascade index visualization (darker = nearer cascade)
    float t = (u_CascadeCount <= 1) ? 0.0 : float(cascadeIdx) / float(u_CascadeCount - 1);
    FragColor = vec4(vec3(t), 1.0);
    return;
#elif DEBUG_VIEW == 3
    // Shadow UV visualization (R,G = uv, B = depth)
    FragColor = vec4(proj01, 1.0);
    return;
#elif DEBUG_VIEW == 4
    // Failure reason visualization:
    // - Magenta: shadow texture not bound/created
    // - Orange : lp.w <= 0 (bad matrix / wrong space)
    // - Red    : proj.xy out of [0..1] (wrong frustum / wrong matrices)
    // - Blue   : proj.z out of [0..1]
    // - Cyan   : cascade idx invalid
    // - Green  : OK (inside map; then brightness still shows shadow)
    if (reason == REASON_TEX_UNBOUND)     { FragColor = vec4(1, 0, 1, 1); return; } // magenta
    if (reason == REASON_BAD_W)           { FragColor = vec4(1, 0.5, 0, 1); return; } // orange
    if (reason == REASON_OUTSIDE_XY)      { FragColor = vec4(1, 0, 0, 1); return; } // red
    if (reason == REASON_OUTSIDE_Z)       { FragColor = vec4(0, 0, 1, 1); return; } // blue
    if (reason == REASON_CASCADE_INVALID) { FragColor = vec4(0, 1, 1, 1); return; } // cyan
    if (reason == REASON_SHADOWS_DISABLED) { FragColor = vec4(0.5,0.5,0.5,1); return; } // gray
    if (reason == REASON_NO_CASCADES)      { FragColor = vec4(0.5,0,0.5,1);   return; } // purple

    // OK: show shadow factor (green channel) to see it changing
    float d = texture(u_ShadowMapArray, vec3(proj01.xy, float(cascadeIdx))).r;
    FragColor = vec4(vec3(d), 1.0);
    return;
#endif

    // Normal lighting (ambient unshadowed, diffuse shadowed)
    vec3 lit = u_Ambient * u_LightColor
             + (1.0 - shadow) * ndl * u_LightColor;

    vec3 color = (u_UseLighting == 1) ? (albedo * lit) : albedo;
    FragColor = vec4(clamp(color, 0.0, 1.0), u_Color.a);
}