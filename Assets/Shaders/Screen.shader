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


uniform float u_Exposure;     // e.g. 1.0
uniform int   u_Tonemap;      // 0=none, 1=reinhard, 2=aces
uniform float u_Vignette;     // 0..1
uniform sampler2D  u_Scene;
uniform usampler2D u_ID;          // R32UI picking texture
uniform uint       u_SelectedID;  // 0 = none
uniform vec3       u_OutlineColor;


vec3 TonemapReinhard(vec3 x) { return x / (1.0 + x); }

// very small ACES fit
vec3 TonemapACES(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x*(a*x+b)) / (x*(c*x+d)+e), 0.0, 1.0);
}

void main() {
    vec3 sceneColor = texture(u_Scene, vUV).rgb;

    // outline logic stays the same, but operate in linear space before gamma
    // ... keep your ID outline code ...

    // Exposure
    sceneColor *= u_Exposure;

    // Tonemap
    if (u_Tonemap == 1) sceneColor = TonemapReinhard(sceneColor);
    else if (u_Tonemap == 2) sceneColor = TonemapACES(sceneColor);

    // Vignette
    if (u_Vignette > 0.0) {
        vec2 p = vUV * 2.0 - 1.0;
        float r = dot(p, p);
        float vig = smoothstep(1.0, 1.0 - u_Vignette, r);
        sceneColor *= vig;
    }

    // Gamma
    sceneColor = pow(sceneColor, vec3(1.0/2.2));
    FragColor = vec4(sceneColor, 1.0);
}