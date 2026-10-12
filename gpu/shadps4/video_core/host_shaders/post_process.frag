// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#version 450

layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D texSampler;

layout (push_constant) uniform settings {
    float gamma;
    bool hdr;
    bool srgb_input;
    float sharpen; // gow3: contrast-adaptive sharpening of the downscaled image, 0 off to 1
} pp;

const float cutoff = 0.0031308, a = 1.055, b = 0.055, d = 12.92;
vec3 gamma(vec3 rgb) {
    return mix(
        a * pow(rgb, vec3(1.0 / (2.4 + 1.0 - pp.gamma))) - b,
        d * rgb / pp.gamma,
        lessThan(rgb, vec3(cutoff))
    );
}

// Exact inverse of gamma() at unit gamma, for buffers that are sRGB encoded but must be sampled
// through a UNORM view because Vulkan has no sRGB variant of the 10-bit format.
vec3 degamma(vec3 rgb) {
    return mix(
        pow(max(rgb + b, 0.0) / a, vec3(2.4)),
        rgb / d,
        lessThan(rgb, vec3(d * cutoff))
    );
}

// gow3: an image larger than the output (render resolution above the window) is averaged
// over each output pixel's footprint instead of read with one bilinear tap, which skips most
// of its pixels and shimmers. A grid of bilinear taps covers the footprint (box filter,
// slightly widened by the bilinear taps); the sRGB view gives linear values to average.
vec4 load(vec2 at) {
    vec4 c = texture(texSampler, at);
    if (pp.srgb_input) c.rgb = degamma(c.rgb);
    return c;
}

vec4 area(vec2 center, vec2 footprint, ivec2 taps) {
    vec4 sum = vec4(0.0);
    for (int y = 0; y < taps.y; ++y) {
        for (int x = 0; x < taps.x; ++x) {
            sum += load(center + footprint * ((vec2(x, y) + 0.5) / vec2(taps) - 0.5));
        }
    }
    return sum / float(taps.x * taps.y);
}

void main() {
    const vec2 size = vec2(textureSize(texSampler, 0));
    const vec2 step = abs(vec2(dFdx(uv).x, dFdy(uv).y)); // one output pixel, in uv
    const vec2 texels = step * size;                       // source texels per output pixel
    vec4 color_linear;
    if (texels.x > 1.01 || texels.y > 1.01) {
        const ivec2 taps = clamp(ivec2(ceil(texels)), ivec2(1), ivec2(4));
        color_linear = area(uv, step, taps);
        if (pp.sharpen > 0.0) {
            // AMD CAS (simplified): sharpen against the 4 neighbors, less where contrast is high.
            const vec3 c = color_linear.rgb;
            const vec3 n = area(uv - vec2(0.0, step.y), step, taps).rgb;
            const vec3 s = area(uv + vec2(0.0, step.y), step, taps).rgb;
            const vec3 w = area(uv - vec2(step.x, 0.0), step, taps).rgb;
            const vec3 e = area(uv + vec2(step.x, 0.0), step, taps).rgb;
            const vec3 mn = min(c, min(min(n, s), min(w, e)));
            const vec3 mx = max(c, max(max(n, s), max(w, e)));
            const vec3 amp = sqrt(clamp(min(mn, 1.0 - mx) / max(mx, 1e-4), 0.0, 1.0));
            const vec3 weight = amp * (-1.0 / mix(8.0, 5.0, pp.sharpen));
            color_linear.rgb = clamp((c + (n + s + w + e) * weight) / (1.0 + 4.0 * weight), 0.0, 1.0);
        }
    } else {
        color_linear = load(uv);
    }
    if (pp.hdr) {
        color = color_linear;
    } else {
        color = vec4(gamma(color_linear.rgb), color_linear.a);
    }
}
