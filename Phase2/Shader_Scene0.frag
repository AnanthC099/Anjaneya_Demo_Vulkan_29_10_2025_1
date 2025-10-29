#version 460 core
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 out_TexCoord;

layout(binding = 1) uniform sampler2D uTextureSampler;

layout(location = 0) out vec4 vFragColor;

layout(binding = 0) uniform MVPMatrix
{
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    vec4 fade;           // x = fade [0..1]
} uMVP;

void main()
{
    vec4 tex = texture(uTextureSampler, out_TexCoord);
    float f = clamp(uMVP.fade.x, 0.0, 1.0);

    // Premultiplied alpha
    vec4 premul = vec4(tex.rgb * tex.a, tex.a);
    vFragColor = vec4(premul.rgb * f, premul.a * f);
}
