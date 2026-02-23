#type vertex
#version 330 core

layout(location=0) in vec3 a_Position;
layout(location=1) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec2 v_TexCoord;

void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
}

#type fragment
#version 330 core

in vec2 v_TexCoord;

uniform sampler2D u_Texture0;
uniform int u_UseTexture;
uniform vec4 u_Color;

out vec4 o_Color;

void main()
{
    vec4 tex = (u_UseTexture != 0) ? texture(u_Texture0, v_TexCoord) : vec4(1.0);
    o_Color = tex * u_Color;
}