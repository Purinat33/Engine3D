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
uniform sampler2D u_ShadowMap;
uniform mat4  u_LightSpaceMatrix;
uniform float u_ShadowBias;

float ShadowFactor(vec3 worldPos, vec3 N, vec3 L) {
    vec4 lp = u_LightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 proj = lp.xyz / lp.w;
    proj = proj * 0.5 + 0.5;

    // outside shadow map => no shadow
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
    return 0.0;

    float current = proj.z;

    // slope-scaled bias helps acne
    float bias = max(u_ShadowBias * (1.0 - dot(N, L)), u_ShadowBias);

    // 3x3 PCF
    vec2 texel = 1.0 / vec2(textureSize(u_ShadowMap, 0));
    float shadow = 0.0;

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float pcfDepth = texture(u_ShadowMap, proj.xy + vec2(x, y) * texel).r;
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

float shadow = 0.0;
if (u_UseShadows == 1)
    shadow = ShadowFactor(v_WorldPos, N, L);

// ambient NOT shadowed, diffuse IS shadowed
vec3 lit = u_Ambient * u_LightColor + (1.0 - shadow) * ndl * u_LightColor;
vec3 color = (u_UseLighting == 1) ? (albedo * lit) : albedo;
FragColor = vec4(clamp(color, 0.0, 1.0), u_Color.a);
}