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

uniform sampler2D  u_Scene;
uniform usampler2D u_ID;          // R32UI picking texture
uniform uint       u_SelectedID;  // 0 = none
uniform vec3       u_OutlineColor;

void main() {
    vec3 sceneColor = texture(u_Scene, vUV).rgb;

    // If nothing selected, just show the scene
    if (u_SelectedID == 0u) {
        sceneColor = pow(sceneColor, vec3(1.0/2.2));
        FragColor = vec4(sceneColor, 1.0);
        return;
    }

    ivec2 size = textureSize(u_ID, 0);
    ivec2 p = ivec2(vUV * vec2(size));
    p = clamp(p, ivec2(0), size - ivec2(1));

    uint c = texelFetch(u_ID, p, 0).r;

    // 4-neighbor edge detect around selected ID
    bool edge = false;
    ivec2 offsets[4] = ivec2[4]( ivec2(1,0), ivec2(-1,0), ivec2(0,1), ivec2(0,-1) );

    for (int i = 0; i < 4; i++) {
        ivec2 q = clamp(p + offsets[i], ivec2(0), size - ivec2(1));
        uint n = texelFetch(u_ID, q, 0).r;

        // outline boundary where selected meets non-selected
        if ((c == u_SelectedID && n != u_SelectedID) || (c != u_SelectedID && n == u_SelectedID))
            edge = true;
    }

    if (edge) {
        vec3 outc = pow(u_OutlineColor, vec3(1.0/2.2));
        FragColor = vec4(outc, 1.0);
        return;
    }

    sceneColor = pow(sceneColor, vec3(1.0/2.2));
    FragColor = vec4(sceneColor, 1.0);
}