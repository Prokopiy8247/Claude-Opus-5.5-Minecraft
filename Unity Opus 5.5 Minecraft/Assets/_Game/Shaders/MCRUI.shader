Shader "MCR/UI"
{
    // Pixel-space interface rendering: vertices arrive in screen pixels (origin top-left) and are written
    // straight to clip space, so no camera projection is involved and the UI is resolution independent.
    // The interface is drawn into an off-screen texture with premultiplied alpha (colour blends normally, alpha
    // accumulates coverage) and composited over the frame by MCR/UIComposite.
    Properties
    {
        _MainTex ("Atlas", 2D) = "white" {}
        _Color ("Color", Color) = (1,1,1,1)
        _ClipRect ("Clip (x0,y0,x1,y1 in pixels)", Vector) = (0,0,0,0)
        _ClipOn ("Clip Enabled", Float) = 0
    }
    SubShader
    {
        Tags { "RenderPipeline" = "UniversalPipeline" "RenderType" = "Transparent" "Queue" = "Overlay" "IgnoreProjector" = "True" }
        Pass
        {
            Name "MCRUI"
            Tags { "LightMode" = "UniversalForward" }
            Cull Off
            ZWrite Off
            ZTest Always
            Blend One OneMinusSrcAlpha
            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"

            TEXTURE2D(_MainTex); SAMPLER(sampler_MainTex);
            float4 _UITexelSize;
            CBUFFER_START(UnityPerMaterial)
                float4 _MainTex_ST;
                float4 _Color;
                float4 _ClipRect;
                float _ClipOn;
            CBUFFER_END
            float4 _UI_Scale;  // (2/width, 2/height, y sign, 0) set globally each frame

            struct Attributes { float3 positionOS : POSITION; float2 uv : TEXCOORD0; half4 color : COLOR; };
            struct Varyings { float4 positionCS : SV_POSITION; float2 uv : TEXCOORD0; half4 color : COLOR; float2 pixel : TEXCOORD1; };

            Varyings vert(Attributes i)
            {
                Varyings o;
                float ySign = _UI_Scale.z < 0 ? -1.0 : 1.0;
                o.positionCS = float4(i.positionOS.x * _UI_Scale.x - 1.0, (1.0 - i.positionOS.y * _UI_Scale.y) * ySign, 0.5, 1.0);
                o.uv = i.uv;
                o.color = i.color * _Color;
                o.pixel = i.positionOS.xy;
                return o;
            }

            half4 frag(Varyings i) : SV_Target
            {
                if (_ClipOn > 0.5)
                {
                    clip(i.pixel.x - _ClipRect.x);
                    clip(_ClipRect.z - i.pixel.x);
                    clip(i.pixel.y - _ClipRect.y);
                    clip(_ClipRect.w - i.pixel.y);
                }
                half4 c = SAMPLE_TEXTURE2D(_MainTex, sampler_MainTex, i.uv);
                half4 v = i.color;
                float a = c.a * v.a;
                // premultiplied output: the colour channel blends normally while alpha accumulates coverage
                return half4(c.rgb * v.rgb * a, a);
            }
            ENDHLSL
        }
    }
    FallBack Off
}
