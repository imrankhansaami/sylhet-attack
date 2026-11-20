// gl3_shaders.h - the GLSL 3.3 programs for the shader renderer
//
// These are the four programs the port needs: the sky, a lit surface with
// normal mapping (ground, walls, roofs, landmarks, aircraft), instanced city
// boxes, and water. They are plain strings so the executable stays a single
// file with no runtime shader assets, exactly like the rest of the project.
//
// Nothing includes this header yet: the fixed-function renderer is still the
// shipping path. It is the foundation for the RENDERER=GL3 build.
#pragma once

static const char* GL3_SKY_VS = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPos;          // fullscreen quad, NDC
out vec2 vNdc;
void main()
{
    vNdc = aPos;
    gl_Position = vec4(aPos, 1.0, 1.0);
}
)GLSL";

static const char* GL3_SKY_FS = R"GLSL(
#version 330 core
in vec2 vNdc;
uniform vec3 uFwd;
uniform vec3 uRight;
uniform vec3 uUp;
uniform float uTanHalfFov;
uniform float uAspect;
uniform vec3 uSunDir;
uniform vec3 uHorizon;
uniform vec3 uZenith;
out vec4 fragColour;
void main()
{
    vec3 ray = normalize(uFwd + uRight * vNdc.x * uTanHalfFov * uAspect
                              + uUp    * vNdc.y * uTanHalfFov);
    float h = clamp(ray.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 sky = mix(uHorizon, uZenith, pow(h, 0.65));
    float sun = max(dot(ray, normalize(uSunDir)), 0.0);
    sky += vec3(1.0, 0.85, 0.6) * pow(sun, 350.0) * 4.0;     // disc
    sky += vec3(1.0, 0.75, 0.45) * pow(sun, 12.0) * 0.18;    // glow
    fragColour = vec4(sky, 1.0);
}
)GLSL";

static const char* GL3_SURFACE_VS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
uniform mat4 uModel;
uniform mat4 uViewProj;
out vec3 vWorld;
out vec3 vNormal;
out vec2 vUV;
void main()
{
    vec4 w = uModel * vec4(aPos, 1.0);
    vWorld = w.xyz;
    vNormal = mat3(uModel) * aNormal;
    vUV = aUV;
    gl_Position = uViewProj * w;
}
)GLSL";

static const char* GL3_SURFACE_FS = R"GLSL(
#version 330 core
in vec3 vWorld;
in vec3 vNormal;
in vec2 vUV;
uniform sampler2D uAlbedo;
uniform sampler2D uNormalMap;
uniform int   uUseNormalMap;
uniform vec3  uTint;
uniform vec3  uSunDir;
uniform vec3  uSunColour;
uniform vec3  uEye;
uniform vec3  uFogColour;
uniform vec2  uFogRange;
uniform float uSpecular;
out vec4 fragColour;

// derivative-based TBN, so no tangent attribute is needed
vec3 bumpNormal(vec3 n, vec3 p, vec2 uv)
{
    vec3 dp1 = dFdx(p), dp2 = dFdy(p);
    vec2 du1 = dFdx(uv), du2 = dFdy(uv);
    vec3 dp2perp = cross(dp2, n);
    vec3 dp1perp = cross(n, dp1);
    vec3 T = dp2perp * du1.x + dp1perp * du2.x;
    vec3 B = dp2perp * du1.y + dp1perp * du2.y;
    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    mat3 TBN = mat3(T * invmax, B * invmax, n);
    vec3 sampled = texture(uNormalMap, uv).xyz * 2.0 - 1.0;
    return normalize(TBN * sampled);
}
void main()
{
    vec3 albedo = texture(uAlbedo, vUV).rgb * uTint;
    vec3 n = normalize(vNormal);
    if (uUseNormalMap != 0) n = bumpNormal(n, vWorld, vUV);

    vec3 L = normalize(uSunDir);
    float ndl = max(dot(n, L), 0.0);
    vec3 diffuse = albedo * uSunColour * ndl;

    // a touch of ambient from the sky, stronger on upward faces
    vec3 ambient = albedo * mix(vec3(0.28, 0.32, 0.36), vec3(0.55, 0.62, 0.70), n.y * 0.5 + 0.5);

    vec3 V = normalize(uEye - vWorld);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(n, H), 0.0), 48.0) * uSpecular;

    vec3 colour = ambient + diffuse + vec3(spec);

    float fog = clamp((length(uEye - vWorld) - uFogRange.x) / (uFogRange.y - uFogRange.x), 0.0, 1.0);
    colour = mix(colour, uFogColour, fog);
    fragColour = vec4(colour, 1.0);
}
)GLSL";

static const char* GL3_CITY_VS = R"GLSL(
#version 330 core
// a unit cube per instance: offset(x,z), size(w,d), height(h), colour
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 iPosSize;      // x, z, w, d
layout(location = 4) in vec2 iHeightTint;   // h, unused
layout(location = 5) in vec3 iColour;
uniform mat4 uViewProj;
out vec3 vWorld;
out vec3 vNormal;
out vec2 vUV;
out vec3 vColour;
void main()
{
    vec3 local = aPos;
    float h = iHeightTint.x;
    vec3 world = vec3(iPosSize.x + local.x * iPosSize.z,
                      (local.y + 0.5) * h,
                      iPosSize.y + local.z * iPosSize.w);
    vWorld = world;
    vNormal = aNormal;
    vUV = vec2(local.x, local.z) * 8.0;
    vColour = iColour;
    gl_Position = uViewProj * vec4(world, 1.0);
}
)GLSL";

static const char* GL3_WATER_VS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uViewProj;
uniform float uTime;
out vec3 vWorld;
void main()
{
    vec3 p = aPos;
    p.y += sin(p.x * 0.05 + uTime * 1.2) * 0.12 + sin(p.z * 0.07 + uTime * 0.9) * 0.10;
    vWorld = p;
    gl_Position = uViewProj * vec4(p, 1.0);
}
)GLSL";

static const char* GL3_WATER_FS = R"GLSL(
#version 330 core
in vec3 vWorld;
uniform sampler2D uNormalMap;
uniform vec3 uEye;
uniform vec3 uSunDir;
uniform vec3 uSunColour;
uniform vec3 uSkyColour;
uniform vec3 uDeepColour;
uniform float uTime;
out vec4 fragColour;
void main()
{
    vec2 uv1 = vWorld.xz * 0.012 + vec2(uTime * 0.015, uTime * 0.008);
    vec2 uv2 = vWorld.xz * 0.031 - vec2(uTime * 0.011, uTime * 0.021);
    vec3 n1 = texture(uNormalMap, uv1).xyz * 2.0 - 1.0;
    vec3 n2 = texture(uNormalMap, uv2).xyz * 2.0 - 1.0;
    vec3 n = normalize(vec3(n1.x + n2.x, 1.0, n1.z + n2.z));

    vec3 V = normalize(uEye - vWorld);
    float fres = pow(1.0 - max(dot(n, V), 0.0), 3.0);
    vec3 colour = mix(uDeepColour, uSkyColour, fres);

    vec3 L = normalize(uSunDir);
    vec3 H = normalize(L + V);
    colour += uSunColour * pow(max(dot(n, H), 0.0), 220.0) * 1.8;

    fragColour = vec4(colour, 1.0);
}
)GLSL";