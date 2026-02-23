#type vertex
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;   // matches your mesh layout
layout(location = 2) in vec2 a_TexCoord; // matches your mesh layout

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

void main() {
    gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
}

#type fragment
#version 330 core

uniform vec4 u_Color;

out vec4 FragColor;

void main() {
    FragColor = u_Color; // unlit, always visible
}