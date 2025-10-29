#version 450

layout(set = 0, binding = 0) uniform UBO {
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;

    // overlayParams:
    // x = fade (0..1)
    // y = screen width  (px)
    // z = screen height (px)
    // w = +size fraction of min(screenW,screenH)  (positive => rectangle, NO circle)
    vec4 overlayParams;
} ubo;

layout(set = 0, binding = 1) uniform samplerCube texCube;   // background
layout(set = 0, binding = 2) uniform sampler2D   texOverlay; // overlay image

layout(location = 0) in  vec3 vDir;    // from vertex shader (skybox dir)
layout(location = 0) out vec4 outColor;

// Fraction of the overlay's smaller dimension used as soft edge width.
// Increase to hide the square boundary more aggressively.
const float kEdgeFeatherFrac = 0.08;  // 8% of overlay size (try 0.05 .. 0.12)

void main()
{
    // Background sample from cubemap
    vec3 dir = normalize(vDir);
    vec3 bg  = texture(texCube, dir).rgb;

    float fade = clamp(ubo.overlayParams.x, 0.0, 1.0);
    if (fade <= 0.0) {
        outColor = vec4(bg, 1.0);
        return;
    }

    // Screen in pixels (Vulkan: gl_FragCoord origin is top-left)
    vec2 screen = vec2(max(1.0, ubo.overlayParams.y), max(1.0, ubo.overlayParams.z));
    vec2 fragPx = gl_FragCoord.xy;

    // Desired overlay size (in pixels), centered on screen, preserving texture aspect
    float frac    = max(0.001, abs(ubo.overlayParams.w));  // fraction of min(screen.x, screen.y)
    float targetM = frac * min(screen.x, screen.y);

    ivec2 texSizeI = textureSize(texOverlay, 0);
    vec2  texSize  = vec2(max(1, texSizeI.x), max(1, texSizeI.y));

    // Scale so the image's *smaller* dimension becomes 'targetM', but never exceed screen (contain)
    float sTarget = targetM / min(texSize.x, texSize.y);
    float sMax    = min(screen.x / texSize.x, screen.y / texSize.y);
    float s       = min(sTarget, sMax);

    vec2 sizePx   = texSize * s;
    vec2 center   = 0.5 * screen;
    vec2 rectMin  = center - 0.5 * sizePx;     // top-left corner of overlay rect
    vec2 uv       = (fragPx - rectMin) / sizePx;

    // If your overlay appears upside-down, uncomment the next line:
    // uv.y = 1.0 - uv.y;

    // Only sample inside the rectangle
    if (all(greaterThanEqual(uv, vec2(0.0))) && all(lessThanEqual(uv, vec2(1.0)))) {
        vec4 overlay = texture(texOverlay, uv);

        // ----- Soft-edge feathering (in pixels) -----
        // Distance in pixels to the nearest rectangle edge
        float dxPx = min(uv.x, 1.0 - uv.x) * sizePx.x;
        float dyPx = min(uv.y, 1.0 - uv.y) * sizePx.y;
        float dPx  = min(dxPx, dyPx);

        // Feather width = fraction of the overlay's smaller dimension
        float featherPx = max(1.0, kEdgeFeatherFrac * min(sizePx.x, sizePx.y));

        // Edge mask: 0 at the border, 1 when 'featherPx' inside the rect
        float edgeMask = smoothstep(0.0, featherPx, dPx);

        // Final alpha uses both time fade and edge feather
        float a = overlay.a * fade * edgeMask;

        outColor = vec4(mix(bg, overlay.rgb, a), 1.0);
    } else {
        outColor = vec4(bg, 1.0);
    }
}
