#ifndef MCR_COMMON_INCLUDED
#define MCR_COMMON_INCLUDED

#include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"

// Global lighting state set every frame by WorldLighting.cs
float4 _MC_Light;       // x = daylight factor for sky light (0.2..1), y = min ambient, z = brightness/gamma boost, w = night vision
float4 _MC_SkyTint;     // rgb tint applied to sky light (bluish at night / dimension ambient)
float4 _MC_BlockTint;   // rgb tint for block light (warm)
float4 _MC_FogColor;    // rgb fog color, a = unused
float4 _MC_FogParams;   // x = start, y = end, z = underwater/lava density mode, w = enabled
float4 _MC_Anim;        // x = animation frame counter (10 fps), y = time seconds

// The original's look is authored in gamma space: face shading, biome tints, the lightmap and fog all multiply
// sRGB-encoded colours. The project renders in linear space, so the block and entity shaders convert the
// sampled texture back to sRGB, do the same arithmetic there, and convert the result to linear at the end.
half3 MC_ToGamma(half3 c)
{
    c = max(c, 0.0h);
    half3 lo = c * 12.92h;
    half3 hi = 1.055h * pow(c, 1.0h / 2.4h) - 0.055h;
    return lerp(hi, lo, step(c, 0.0031308h));
}

half3 MC_ToLinear(half3 c)
{
    c = max(c, 0.0h);
    half3 lo = c / 12.92h;
    half3 hi = pow((c + 0.055h) / 1.055h, 2.4h);
    return lerp(hi, lo, step(c, 0.04045h));
}

// Minecraft-like lightmap curve (level in 0..1)
half LightCurve(half l)
{
    return l / (4.0h - 3.0h * l);
}

half3 ComputeLight(half sky, half blk)
{
    half s = LightCurve(saturate(sky * _MC_Light.x));
    half b = LightCurve(saturate(blk));
    half3 skyC = s * _MC_SkyTint.rgb;
    // block light flickers slightly warmer when bright
    half3 blkC = b * _MC_BlockTint.rgb;
    half3 L = max(skyC, blkC);
    L = max(L, _MC_Light.yyy);
    // brightness ("gamma") slider, MC style: lerp toward inverted-cubic curve
    half3 inv = 1.0h - L;
    half3 bright = 1.0h - inv * inv * inv * inv;
    L = lerp(L, bright, _MC_Light.z);
    L = lerp(L, max(L, 0.85h), _MC_Light.w);
    return saturate(L);
}

// Open-air fog is a cylinder around the camera (horizontal distance, or the height difference when that is
// larger), so looking down from a mountain stays clear and only the edge of the render distance hazes over.
// The dense fogs under water, in lava and in powder snow are spherical.
float MCFogDistance(float3 positionWS)
{
    float3 d = positionWS - _WorldSpaceCameraPos.xyz;
    return _MC_FogParams.z > 0.5 ? length(d) : max(length(d.xz), abs(d.y));
}

// fog with its distances stretched by `scale` (clouds and sky elements fade much further out than terrain)
float ComputeMCFogScaled(float3 positionWS, float scale)
{
    float d = MCFogDistance(positionWS) / max(0.001, scale);
    float f = saturate((d - _MC_FogParams.x) / max(0.001, _MC_FogParams.y - _MC_FogParams.x));
    return f * _MC_FogParams.w;
}

float ComputeMCFog(float3 positionWS)
{
    float d = MCFogDistance(positionWS);
    float f = saturate((d - _MC_FogParams.x) / max(0.001, _MC_FogParams.y - _MC_FogParams.x));
    return f * _MC_FogParams.w;
}

#endif
