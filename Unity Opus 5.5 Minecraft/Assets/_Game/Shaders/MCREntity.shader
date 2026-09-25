Shader "MCR/Entity"
{
    Properties
    {
        _MainTex ("Skin", 2D) = "white" {}
        _Color ("Color", Color) = (1,1,1,1)
        _Cutoff ("Alpha Cutoff", Range(0,1)) = 0.1
        _EntityLight ("Light (sky, block, unused, flash)", Vector) = (1,0,1,0)
        _Overlay ("Overlay (rgb, amount)", Vector) = (1,0,0,0)
        _Emission ("Emission (glowing eyes etc.)", Float) = 0
        [Enum(UnityEngine.Rendering.CullMode)] _Cull ("Cull", Float) = 2
        [Enum(UnityEngine.Rendering.BlendMode)] _SrcBlend ("Src Blend", Float) = 1
        [Enum(UnityEngine.Rendering.BlendMode)] _DstBlend ("Dst Blend", Float) = 0
        [Toggle] _ZWrite ("ZWrite", Float) = 1
    }
    SubShader
    {
        Tags { "RenderPipeline" = "UniversalPipeline" "RenderType" = "Opaque" "Queue" = "Geometry+10" }
        Pass
        {
            Name "MCREntity"
            Tags { "LightMode" = "UniversalForward" }
            Cull [_Cull]
            ZWrite [_ZWrite]
            Blend [_SrcBlend] [_DstBlend]
            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #pragma target 3.0
            #include "MCRCommon.hlsl"

            TEXTURE2D(_MainTex); SAMPLER(sampler_MainTex);
            CBUFFER_START(UnityPerMaterial)
                float4 _MainTex_ST;
                float4 _Color;
                float _Cutoff;
                float4 _EntityLight;
                float4 _Overlay;
                float _Emission;
                float _Cull; float _SrcBlend; float _DstBlend; float _ZWrite;
            CBUFFER_END

            struct Attributes { float3 positionOS : POSITION; float3 normalOS : NORMAL; float2 uv : TEXCOORD0; half4 color : COLOR; };
            struct Varyings { float4 positionCS : SV_POSITION; float2 uv : TEXCOORD0; half3 light : TEXCOORD1; float fog : TEXCOORD2; half shade : TEXCOORD3; half4 color : COLOR; };

            Varyings vert(Attributes i)
            {
                Varyings o;
                float3 ws = TransformObjectToWorld(i.positionOS);
                o.positionCS = TransformWorldToHClip(ws);
                o.uv = i.uv;
                float3 n = normalize(TransformObjectToWorldNormal(i.normalOS));
                // Minecraft-style fixed two-light diffuse
                float3 l0 = normalize(float3(0.2, 1.0, -0.7));
                float3 l1 = normalize(float3(-0.2, 1.0, 0.7));
                half d = saturate(dot(n, l0)) + saturate(dot(n, l1));
                o.shade = saturate(0.6 * d * 0.6 + 0.4 + 0.25 * saturate(n.y));
                o.light = ComputeLight((half)_EntityLight.x, (half)_EntityLight.y);
                o.fog = ComputeMCFog(ws);
                o.color = i.color;
                return o;
            }

            half4 frag(Varyings i) : SV_Target
            {
                half4 tex = SAMPLE_TEXTURE2D(_MainTex, sampler_MainTex, i.uv);
                clip(tex.a - _Cutoff);
                // shade in gamma space like the blocks (see MCRCommon.hlsl); _Color arrives linear from the material
                half3 rgb = MC_ToGamma(tex.rgb) * MC_ToGamma(_Color.rgb);
                half3 lit = rgb * i.shade * i.light;
                // emissive pixels: skins mark "glow" pixels with alpha in (0.5..0.99)
                half glow = (tex.a < 0.99h && tex.a > 0.5h) ? 1.0h : 0.0h;
                glow = max(glow, (half)_Emission);
                rgb = lerp(lit, rgb, glow);
                rgb = lerp(rgb, _Overlay.rgb, (half)_Overlay.a);
                rgb = lerp(rgb, half3(1,1,1), (half)_EntityLight.w);
                rgb = lerp(rgb, MC_ToGamma(_MC_FogColor.rgb), i.fog);
                return half4(MC_ToLinear(rgb), _Color.a * (tex.a > 0.5h ? 1.0h : tex.a));
            }
            ENDHLSL
        }
    }
    FallBack Off
}
