Shader "MCR/Chunk"
{
    Properties
    {
        _BlockTex ("Block Texture Array", 2DArray) = "" {}
        _Cutoff ("Alpha Cutoff", Range(0,1)) = 0.5
        [Enum(UnityEngine.Rendering.CullMode)] _Cull ("Cull", Float) = 2
        [Enum(UnityEngine.Rendering.BlendMode)] _SrcBlend ("Src Blend", Float) = 1
        [Enum(UnityEngine.Rendering.BlendMode)] _DstBlend ("Dst Blend", Float) = 0
        [Toggle] _ZWrite ("ZWrite", Float) = 1
        _Mode ("Mode (0 opaque 1 cutout 2 translucent)", Float) = 0
        _EntityLight ("Entity Light (sky, block, use, flash)", Vector) = (1,0,0,0)
        _Tint ("Tint", Color) = (1,1,1,1)
    }
    SubShader
    {
        Tags { "RenderPipeline" = "UniversalPipeline" "RenderType" = "Opaque" "Queue" = "Geometry" }
        Pass
        {
            Name "MCRForward"
            Tags { "LightMode" = "UniversalForward" }
            Cull [_Cull]
            ZWrite [_ZWrite]
            Blend [_SrcBlend] [_DstBlend]

            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #pragma target 3.5
            #pragma require 2darray
            #include "MCRCommon.hlsl"

            TEXTURE2D_ARRAY(_BlockTex);
            SAMPLER(sampler_BlockTex);

            CBUFFER_START(UnityPerMaterial)
                float _Cutoff;
                float _Mode;
                float _Cull;
                float _SrcBlend;
                float _DstBlend;
                float _ZWrite;
                float4 _EntityLight;
                float4 _Tint;
            CBUFFER_END

            struct Attributes
            {
                float3 positionOS : POSITION;
                half4 color : COLOR;
                float4 uv : TEXCOORD0;     // u, v, layer, frames
                half4 light : TEXCOORD1;   // sky, block
            };

            struct Varyings
            {
                float4 positionCS : SV_POSITION;
                half4 color : COLOR;
                float3 uv : TEXCOORD0;
                half3 light : TEXCOORD1;
                float fog : TEXCOORD2;
            };

            Varyings vert(Attributes i)
            {
                Varyings o;
                float3 ws = TransformObjectToWorld(i.positionOS);
                o.positionCS = TransformWorldToHClip(ws);
                float layer = i.uv.z;
                float frames = max(1.0, i.uv.w);
                layer += fmod(_MC_Anim.x, frames);
                o.uv = float3(i.uv.xy, layer);
                o.color = i.color;
                half sky = lerp(i.light.x, (half)_EntityLight.x, (half)_EntityLight.z);
                half blk = lerp(i.light.y, (half)_EntityLight.y, (half)_EntityLight.z);
                o.light = ComputeLight(sky, blk);
                o.fog = ComputeMCFog(ws);
                return o;
            }

            half4 frag(Varyings i) : SV_Target
            {
                half4 tex = SAMPLE_TEXTURE2D_ARRAY(_BlockTex, sampler_BlockTex, i.uv.xy, i.uv.z);
                tex.rgb = MC_ToGamma(tex.rgb);   // shade in gamma space, like the original (see MCRCommon.hlsl)
                half3 tint = i.color.rgb;
                half shade = i.color.a;
                half3 rgb;
                half alpha = 1;
                if (_Mode < 0.5)
                {
                    // opaque: alpha encodes tint mask (1 = untinted, 0.5 = tinted)
                    half tintAmt = saturate((1.0h - tex.a) * 2.0h);
                    rgb = tex.rgb * lerp(half3(1,1,1), tint, tintAmt);
                }
                else
                {
                    rgb = tex.rgb * tint;
                    alpha = tex.a;
                    if (_Mode < 1.5) clip(alpha - _Cutoff);
                }
                rgb *= shade * i.light * _Tint.rgb;
                // hurt / TNT flash overlay
                rgb = lerp(rgb, half3(1,1,1), (half)_EntityLight.w);
                rgb = lerp(rgb, MC_ToGamma(_MC_FogColor.rgb), i.fog);
                return half4(MC_ToLinear(rgb), alpha * _Tint.a);
            }
            ENDHLSL
        }
    }
    FallBack Off
}
