#type vertex
#version 330 core
layout(location=0) in vec3 aPos;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec3 vWorldPos;

void main() {
    vec4 world = u_Model * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    gl_Position = u_ViewProjection * world;
}

#type fragment
#version 330 core
in vec3 vWorldPos;
out vec4 FragColor;

uniform float u_GridScale;     // e.g. 1.0
uniform vec3  u_GridColor;     // line color
uniform vec3  u_BaseColor;     // base fill color

float gridLine(vec2 coord) {
    // Antialiased grid using derivatives
    vec2 g = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
    float line = 1.0 - min(min(g.x, g.y), 1.0);
    return line;
}

void main() {
    vec2 c = vWorldPos.xz * u_GridScale;

    float line = gridLine(c);

    // Emphasize major lines every 10 cells
    vec2 major = vWorldPos.xz * (u_GridScale / 10.0);
    float majorLine = gridLine(major);
    line = max(line * 0.6, majorLine);

    vec3 col = mix(u_BaseColor, u_GridColor, line);

    FragColor = vec4(col, 1.0);
}