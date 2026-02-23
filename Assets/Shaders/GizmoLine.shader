#type vertex
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;

uniform mat4 u_ViewProjection;

out vec3 vColor;

void main() {
    vColor = aColor;
    gl_Position = u_ViewProjection * vec4(aPos, 1.0);
}

#type fragment
#version 330 core
in vec3 vColor;
out vec4 FragColor;

uniform float u_Opacity;

void main() {
    FragColor = vec4(vColor, u_Opacity);
}