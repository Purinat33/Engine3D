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

uniform int  u_UseLighting;
uniform vec3 u_LightDir;    // direction the light points (world)
uniform vec3 u_LightColor;
uniform float u_Ambient;

uniform int   u_UseShadows;
uniform sampler2DArray u_ShadowMapArray;
uniform int   u_CascadeCount;
uniform float u_CascadeSplits[4];
uniform mat4  u_LightSpaceMatrices[4];
uniform float u_ShadowBias;

uniform mat4 u_View;

float ShadowFactor(int cascadeIdx, vec3 worldPos, vec3 N, vec3 L)
{
    mat4 lightMat = u_LightSpaceMatrices[cascadeIdx];
    vec4 lp = lightMat * vec4(worldPos, 1.0);

    vec3 proj = lp.xyz / lp.w;
    proj = proj * 0.5 + 0.5;

    // outside shadow map => no shadow
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 0.0;

    float current = proj.z;

    // slope-scaled bias helps acne
    float bias = max(u_ShadowBias * (1.0 - dot(N, L)), u_ShadowBias);

    ivec3 ts = textureSize(u_ShadowMapArray, 0);
    vec2 texel = 1.0 / vec2(ts.xy);

    // 3x3 PCF
    float shadow = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float pcfDepth = texture(
                u_ShadowMapArray,
                vec3(proj.xy + vec2(x, y) * texel, float(cascadeIdx))
            ).r;

            shadow += (current - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
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

int cascadeIdx = 0;

// TEMP (OK for now): use world distance as proxy.
// Better later: camera-space depth using u_View or u_CameraPos.
float viewDepth = -(u_View * vec4(v_WorldPos, 1.0)).z;

if (u_CascadeCount > 1 && viewDepth > u_CascadeSplits[0]) cascadeIdx = 1;
if (u_CascadeCount > 2 && viewDepth > u_CascadeSplits[1]) cascadeIdx = 2;
if (u_CascadeCount > 3 && viewDepth > u_CascadeSplits[2]) cascadeIdx = 3;

float shadow = 0.0;
if (u_UseShadows == 1)
    shadow = ShadowFactor(cascadeIdx, v_WorldPos, N, L);

// ambient NOT shadowed, diffuse IS shadowed
vec3 lit = u_Ambient * u_LightColor + (1.0 - shadow) * ndl * u_LightColor;
vec3 color = (u_UseLighting == 1) ? (albedo * lit) : albedo;
FragColor = vec4(clamp(color, 0.0, 1.0), u_Color.a);
}