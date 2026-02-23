#type vertex
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec3 v_NormalWS;
out vec2 v_TexCoord;

void main() {
    mat3 normalMat = transpose(inverse(mat3(u_Model)));
    v_NormalWS = normalize(normalMat * a_Normal);
    v_TexCoord = a_TexCoord;

    gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
}

#type fragment
#version 330 core
in vec3 v_NormalWS;
in vec2 v_TexCoord;

uniform vec4 u_Color;
uniform int  u_UseTexture;
uniform sampler2D u_Texture0;

uniform int  u_UseLighting;
uniform vec3 u_LightDir;    // direction the light points (world)
uniform vec3 u_LightColor;
uniform float u_Ambient;

out vec4 FragColor;

void main() {
    vec3 albedo = u_Color.rgb;
    if (u_UseTexture == 1)
        albedo *= texture(u_Texture0, v_TexCoord).rgb;

    vec3 N = normalize(v_NormalWS);

    // If u_LightDir is "direction light points", the incoming light is opposite:
    vec3 L = normalize(-u_LightDir);

    float ndl = max(dot(N, L), 0.0);
    vec3 lit = u_Ambient * u_LightColor + ndl * u_LightColor;

    vec3 color = (u_UseLighting == 1) ? (albedo * lit) : albedo;
    FragColor = vec4(clamp(color, 0.0, 1.0), u_Color.a);
}