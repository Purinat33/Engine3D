#type vertex
#version 330 core
layout(location=0) in vec3 aPos;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

void main() {
    gl_Position = u_ViewProjection * u_Model * vec4(aPos, 1.0);
}

#type fragment
#version 330 core
layout(location=0) out uint o_ID;
uniform uint u_EntityID;

void main() {
    o_ID = u_EntityID;
}