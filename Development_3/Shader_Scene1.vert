#version 450

layout(location = 0) in vec3 inPos; // cube positions
layout(location = 1) in vec2 inUV;  // present but unused for cubemap

layout(set = 0, binding = 0) uniform UBO {
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    vec4 overlayParams;   // x=fade, y=screenW, z=screenH, w=signed size (sign encodes shape)
} ubo;

layout(location = 0) out vec3 vDir;

void main()
{
    // Remove translation (CPU side already zeroes it; this is extra safety)
    mat4 viewNoT = ubo.viewMatrix;
    viewNoT[3] = vec4(0.0, 0.0, 0.0, 1.0);

    // Standard skybox trick: keep the cube at eye by forcing z = w
    vec4 pos = ubo.projectionMatrix * viewNoT * ubo.modelMatrix * vec4(inPos, 1.0);
    gl_Position = vec4(pos.xy, pos.w, pos.w);

    // Direction for cubemap sampling in WORLD space:
    // Since view is a pure rotation, inverse(view) = transpose(view).
    mat3 RviewInv = transpose(mat3(ubo.viewMatrix));
    vDir = RviewInv * (mat3(ubo.modelMatrix) * inPos);
}
