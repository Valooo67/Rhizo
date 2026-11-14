#version 450
layout(set=0,binding=0) uniform sampler2D hdrColor;
layout(set=0,binding=1) uniform sampler2D sceneDepth;
layout(set=0,binding=2) uniform sampler2D historyColor;
layout(set=0,binding=3) uniform sampler2D historyDepth;
layout(std140,set=0,binding=4) uniform TemporalSettings { mat4 previousViewProjection; uint effectFlags; } temporal;
layout(push_constant) uniform CompositePush {
    mat4 viewProjection;
    mat4 inverseViewProjection;
} pc;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outColor;

bool effectEnabled(uint flag) { return (temporal.effectFlags & flag) != 0u; }
vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
float luminance(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

vec3 reconstructWorld(vec2 coord, float depth) {
    vec4 world = pc.inverseViewProjection * vec4(coord * 2.0 - 1.0, depth, 1.0);
    return world.xyz / max(abs(world.w), 0.000001) * sign(world.w);
}
vec2 projectUv(vec3 world, out float depth) {
    vec4 clip = pc.viewProjection * vec4(world, 1.0);
    vec3 ndc = clip.xyz / max(abs(clip.w), 0.000001) * sign(clip.w);
    depth = ndc.z;
    return ndc.xy * 0.5 + 0.5;
}

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
            float knee = smoothstep(0.72, 1.65, luminance(sampleColor));
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

vec3 screenSpaceGI(vec3 worldPosition, vec3 normal, float centerDepth, vec2 texel) {
    const vec2 directions[8] = vec2[8](
        vec2(1,0), vec2(-1,0), vec2(0,1), vec2(0,-1),
        vec2(0.707,0.707), vec2(-0.707,0.707),
        vec2(0.707,-0.707), vec2(-0.707,-0.707)
    );
    vec3 indirect = vec3(0.0);
    float totalWeight = 0.0;
    for (int i=0; i<8; ++i) {
        vec2 sampleUv = uv + directions[i] * texel * 5.0;
        float sampleDepth = texture(sceneDepth, sampleUv).r;
        if (sampleDepth >= 0.99999) continue;
        vec3 sampleWorld = reconstructWorld(sampleUv, sampleDepth);
        vec3 delta = sampleWorld - worldPosition;
        float distanceWeight = 1.0 / (1.0 + dot(delta, delta) * 0.08);
        float depthWeight = exp(-abs(sampleDepth-centerDepth) * 90.0);
        float hemisphereWeight = max(dot(normal, normalize(delta)), 0.0);
        float weight = distanceWeight * depthWeight * hemisphereWeight;
        indirect += max(texture(hdrColor, sampleUv).rgb, vec3(0.0)) * weight;
        totalWeight += weight;
    }
    return totalWeight > 0.0001 ? indirect / totalWeight : vec3(0.0);
}

vec3 screenSpaceReflection(vec3 worldPosition, vec3 normal, vec3 cameraRay, float roughnessProxy) {
    vec3 reflected = normalize(reflect(cameraRay, normal));
    vec3 hitColor = vec3(0.0);
    float hit = 0.0;
    for (int stepIndex=1; stepIndex<=20; ++stepIndex) {
        float distanceAlongRay = 0.035 * float(stepIndex);
        vec3 rayPosition = worldPosition + normal * 0.015 + reflected * distanceAlongRay;
        float rayDepth;
        vec2 rayUv = projectUv(rayPosition, rayDepth);
        if (any(lessThan(rayUv, vec2(0.002))) || any(greaterThan(rayUv, vec2(0.998)))) break;
        float sceneZ = texture(sceneDepth, rayUv).r;
        if (rayDepth > sceneZ + 0.0015 && sceneZ < 0.9999) {
            hitColor = texture(hdrColor, rayUv).rgb;
            hit = 1.0 - float(stepIndex) / 24.0;
            break;
        }
    }
    return max(hitColor, vec3(0.0)) * hit * (1.0 - roughnessProxy) * 0.22;
}

void main() {
    vec2 texel = 1.0 / vec2(textureSize(hdrColor, 0));
    vec4 hdr = texture(hdrColor, uv);
    float depth = texture(sceneDepth, uv).r;
    vec3 scene = max(hdr.rgb, vec3(0.0));
    vec3 worldPosition = reconstructWorld(uv, depth);
    if (depth < 0.99999) {
        vec3 rightPosition = reconstructWorld(uv + vec2(texel.x,0.0), texture(sceneDepth,uv+vec2(texel.x,0.0)).r);
        vec3 upPosition = reconstructWorld(uv + vec2(0.0,texel.y), texture(sceneDepth,uv+vec2(0.0,texel.y)).r);
        vec3 normal = normalize(cross(rightPosition-worldPosition, upPosition-worldPosition));
        if (!gl_FrontFacing) normal = -normal;
        vec3 cameraRay = normalize(worldPosition - reconstructWorld(uv, 0.0));
        if (effectEnabled(2u)) scene *= screenSpaceAO(depth, texel);
        if (effectEnabled(8u)) scene += screenSpaceGI(worldPosition, normal, depth, texel) * 0.12;
        if (effectEnabled(4u)) scene += screenSpaceReflection(worldPosition, normal, cameraRay, 0.35);
        if (effectEnabled(16u)) scene = depthOfField(scene, depth, texel);
    }
    if (effectEnabled(32u) || effectEnabled(64u)) {
        if ((temporal.effectFlags & 0x80000000u) != 0u && depth < 0.99999) {
            vec4 previousClip = temporal.previousViewProjection * vec4(worldPosition, 1.0);
            vec3 previousNdc = previousClip.xyz / max(abs(previousClip.w), 0.000001) * sign(previousClip.w);
            vec2 previousUv = previousNdc.xy * 0.5 + 0.5;
            float previousDepth = previousNdc.z;
            vec2 velocity = uv - previousUv;
            float velocityLength = length(velocity);
            if (effectEnabled(64u) && velocityLength > 0.0005 && velocityLength < 0.08) {
                vec3 motionBlur = scene * 0.2;
                for (int sampleIndex=1; sampleIndex<=4; ++sampleIndex) {
                    float t = float(sampleIndex) / 4.0;
                    motionBlur += texture(hdrColor, uv - velocity*t).rgb * 0.1;
                    motionBlur += texture(hdrColor, uv + velocity*t).rgb * 0.1;
                }
                scene = mix(scene, motionBlur, clamp(velocityLength * 24.0, 0.0, 0.42));
            }
            if (effectEnabled(32u) && all(greaterThan(previousUv, vec2(0.001))) && all(lessThan(previousUv, vec2(0.999)))) {
                float oldDepth = texture(historyDepth, previousUv).r;
                float depthTolerance = 0.0015 + abs(previousDepth) * 0.004;
                if (abs(oldDepth - previousDepth) < depthTolerance) {
                    vec3 neighborhoodMin = scene;
                    vec3 neighborhoodMax = scene;
                    for (int y=-1; y<=1; ++y) for (int x=-1; x<=1; ++x) {
                        vec3 neighbor = texture(hdrColor, uv + vec2(x,y)*texel).rgb;
                        neighborhoodMin = min(neighborhoodMin, neighbor);
                        neighborhoodMax = max(neighborhoodMax, neighbor);
                    }
                    vec3 history = clamp(texture(historyColor, previousUv).rgb, neighborhoodMin, neighborhoodMax);
                    scene = mix(scene, history, 0.88);
                }
            }
        }
    }
    if (effectEnabled(1u)) scene += bloomContribution(texel);
    vec3 mapped = aces(scene);
    mapped = pow(max(mapped, vec3(0.0)), vec3(1.0/2.2));
    outColor = vec4(mapped, hdr.a);
}
