#type vertex
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec2 v_TexCoord;

void main()
{
    v_TexCoord = aTexCoord;
    gl_Position = u_ViewProjection * u_Model * vec4(aPos, 1.0);
}

#type fragment
#version 330 core
in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_Texture0;

void main()
{
    FragColor = texture(u_Texture0, v_TexCoord);
}