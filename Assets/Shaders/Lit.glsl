#type vertex
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec3 vNormal;
out vec3 vWorldPos;

void main()
{
    vec4 world = u_Model * vec4(aPos, 1.0);
    vWorldPos = world.xyz;

    mat3 normalMat = mat3(transpose(inverse(u_Model)));
    vNormal = normalize(normalMat * aNormal);

    gl_Position = u_ViewProjection * world;
}

#type fragment
#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;

out vec4 FragColor;

uniform vec3 u_LightDir;   // direction *toward* the surface (e.g. normalized)
uniform vec3 u_LightColor;
uniform vec3 u_BaseColor;

void main()
{
    vec3 N = normalize(vNormal);
    float ndl = max(dot(N, normalize(u_LightDir)), 0.0);

    vec3 diffuse = u_BaseColor * u_LightColor * ndl;
    vec3 ambient = u_BaseColor * 0.15;

    FragColor = vec4(ambient + diffuse, 1.0);
}