#type vertex
#version 330 core
layout(location = 0) in vec3 aPos;

out vec3 vDir;

uniform mat4 u_ViewProjectionNoTranslate;

void main() {
    vDir = aPos;
    vec4 p = u_ViewProjectionNoTranslate * vec4(aPos, 1.0);

    // Force depth to far plane
    gl_Position = p.xyww;
}

#type fragment
#version 330 core
in vec3 vDir;
out vec4 FragColor;

uniform samplerCube u_Skybox;

void main() {
    vec3 c = texture(u_Skybox, vDir).rgb;
    FragColor = vec4(c, 1.0);
}