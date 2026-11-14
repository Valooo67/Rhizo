#version 450
layout(set=0,binding=0) uniform sampler2D hdrColor;
layout(set=0,binding=1) uniform sampler2D sceneDepth;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outColor;

vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
float luminance(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

float screenSpaceAO(float depth, vec2 texel) {
    if (depth >= 0.99999) return 1.0;
    const vec2 offsets[8] = vec2[8](
        vec2(1,0), vec2(-1,0), vec2(0,1), vec2(0,-1),
        vec2(0.707,0.707), vec2(-0.707,0.707),
        vec2(0.707,-0.707), vec2(-0.707,-0.707)
    );
    float occlusion = 0.0;
    for (int i=0; i<8; ++i) {
        float sampleDepth = texture(sceneDepth, uv + offsets[i] * texel * 2.0).r;
        float rangeWeight = smoothstep(0.0, 0.035, depth - sampleDepth);
        occlusion += rangeWeight * step(0.0008, depth - sampleDepth);
    }
    return clamp(1.0 - occlusion * 0.075, 0.48, 1.0);
}

vec3 bloomContribution(vec2 texel) {
    vec3 sum = vec3(0.0);
    float weightSum = 0.0;
    for (int y=-2; y<=2; ++y) {
        for (int x=-2; x<=2; ++x) {
            vec2 offset = vec2(x,y) * texel * 2.0;
            vec3 sampleColor = max(texture(hdrColor, uv + offset).rgb, vec3(0.0));
            float brightness = luminance(sampleColor);
            float knee = smoothstep(0.72, 1.65, brightness);
            float weight = (x==0 && y==0) ? 1.0 : ((abs(x)+abs(y)==1) ? 0.72 : ((abs(x)==1 && abs(y)==1) ? 0.48 : 0.24));
            sum += sampleColor * knee * weight;
            weightSum += weight;
        }
    }
    return sum / max(weightSum, 0.001) * 0.9;
}

vec3 depthOfField(vec3 source, float depth, vec2 texel) {
    float coc = smoothstep(0.12, 0.92, depth) * 2.25;
    if (coc < 0.08) return source;
    vec2 r = texel * coc;
    vec3 blur = source * 0.24;
    blur += texture(hdrColor, uv + vec2( r.x, 0.0)).rgb * 0.12;
    blur += texture(hdrColor, uv + vec2(-r.x, 0.0)).rgb * 0.12;
    blur += texture(hdrColor, uv + vec2(0.0,  r.y)).rgb * 0.12;
    blur += texture(hdrColor, uv + vec2(0.0, -r.y)).rgb * 0.12;
    blur += texture(hdrColor, uv + r).rgb * 0.07;
    blur += texture(hdrColor, uv - r).rgb * 0.07;
    blur += texture(hdrColor, uv + vec2(r.x,-r.y)).rgb * 0.07;
    blur += texture(hdrColor, uv + vec2(-r.x,r.y)).rgb * 0.07;
    return mix(source, blur, clamp(coc * 0.42, 0.0, 0.58));
}

void main() {
    vec2 texel = 1.0 / vec2(textureSize(hdrColor, 0));
    vec4 hdr = texture(hdrColor, uv);
    float depth = texture(sceneDepth, uv).r;
    vec3 scene = max(hdr.rgb, vec3(0.0));
    float ao = screenSpaceAO(depth, texel);
    scene *= ao;
    scene = depthOfField(scene, depth, texel);
    scene += bloomContribution(texel);
    vec3 mapped = aces(scene);
    mapped = pow(max(mapped, vec3(0.0)), vec3(1.0/2.2));
    outColor = vec4(mapped, hdr.a);
}
