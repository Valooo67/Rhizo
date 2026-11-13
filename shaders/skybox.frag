#version 450
layout(push_constant) uniform SkyPush {
    mat4 inverseViewProjection;
    vec4 cameraPosition;
    vec4 viewport;
} pc;
layout(location = 0) out vec4 outColor;
void main() {
    vec2 ndc = (gl_FragCoord.xy / max(pc.viewport.xy, vec2(1.0))) * 2.0 - 1.0;
    vec4 farPoint = pc.inverseViewProjection * vec4(ndc, 1.0, 1.0);
    vec3 direction = normalize(farPoint.xyz / max(abs(farPoint.w), 1.0e-6) - pc.cameraPosition.xyz);
    float horizon = smoothstep(-0.18, 0.72, direction.y);
    vec3 sky = mix(vec3(0.19, 0.24, 0.32), vec3(0.025, 0.075, 0.17), pow(horizon, 0.72));
    float sunDot = max(dot(direction, normalize(vec3(-0.42, 0.82, 0.31))), 0.0);
    sky += vec3(1.0, 0.78, 0.48) * (pow(sunDot, 420.0) * 5.0 + pow(sunDot, 18.0) * 0.16);
    if (direction.y < 0.0) sky = mix(sky, vec3(0.055, 0.052, 0.047), smoothstep(-0.55, -0.04, -direction.y));
    outColor = vec4(pow(max(sky, vec3(0.0)), vec3(1.0 / 2.2)), 1.0);
}
