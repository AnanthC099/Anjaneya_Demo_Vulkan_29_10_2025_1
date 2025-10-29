#version 460 core
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vTexCoord;

layout(location = 0) out vec2 out_TexCoord;

layout(binding = 0) uniform MVPMatrix
{
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    vec4 fade;           // x = fade [0..1]
} uMVP;

void main()
{
    gl_Position = uMVP.projectionMatrix * uMVP.viewMatrix * uMVP.modelMatrix * vPosition;
    out_TexCoord = vec2(vTexCoord.x, 1.0 - vTexCoord.y);
}
