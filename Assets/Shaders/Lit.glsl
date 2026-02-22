#type vertex
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec3 vNormal;
out vec2 vUV;

void main()
{
    mat3 normalMat = mat3(transpose(inverse(u_Model)));
    vNormal = normalize(normalMat * aNormal);
    vUV = aUV;

    gl_Position = u_ViewProjection * u_Model * vec4(aPos, 1.0);
}

#type fragment
#version 330 core
in vec3 vNormal;
in vec2 vUV;

out vec4 FragColor;

uniform vec3 u_LightDir;     // direction toward surface
uniform vec3 u_LightColor;

uniform vec4 u_Color;        // base/tint color (you already set this from Material)
uniform sampler2D u_Texture0;
uniform int u_UseTexture;    // 1 = sample, 0 = ignore

void main()
{
    vec3 albedo = u_Color.rgb;
    if (u_UseTexture == 1)
        albedo *= texture(u_Texture0, vUV).rgb;

    vec3 N = normalize(vNormal);
    float ndl = max(dot(N, normalize(u_LightDir)), 0.0);

    vec3 diffuse = albedo * u_LightColor * ndl;
    vec3 ambient = albedo * 0.15;

    FragColor = vec4(ambient + diffuse, 1.0);
}