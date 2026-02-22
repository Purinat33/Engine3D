#type vertex
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}

#type fragment
#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D u_Scene;

void main() {
    vec3 c = texture(u_Scene, vUV).rgb;
    // optional gamma correction (sRGB-ish)
    c = pow(c, vec3(1.0/2.2));
    FragColor = vec4(c, 1.0);
}