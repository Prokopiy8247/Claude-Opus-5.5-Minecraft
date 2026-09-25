// Editor-only procedural asset generation.
//
// The runtime synthesises its textures and terrain, so what it needs from /Game is:
//   * the material graphs that consume the runtime texture arrays (parameter names must match
//     what MCVoxelRenderer / MCAssets / MCRig set on their MIDs),
//   * a default map (GameMode = MCGameMode),
//   * the Blender-authored hero meshes, imported from Saved/Opus55Fbx.
#include "MCEditorLibrary.h"
#include "Unreal_MinecraftEditor.h"

#include "AssetImportTask.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "AssetToolsModule.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/WorldFactory.h"
#include "FileHelpers.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Engine/Texture2DArray.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Game/MCGameMode.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "MeshDescription.h"
#include "Render/MCRig.h"

namespace MCContent
{
	const TCHAR* MatPath = TEXT("/Game/Opus55Minecraft/Materials");
	const TCHAR* MapPath = TEXT("/Game/Opus55Minecraft/Maps");
	const TCHAR* MapName = TEXT("L_Opus55World");
	const TCHAR* LayerNames[] = { TEXT("Opaque"), TEXT("Cutout"), TEXT("Translucent"), TEXT("Water"), TEXT("Lava"), TEXT("Portal"), TEXT("EndPortal") };

	IAssetTools& Tools() { return FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get(); }

	bool SaveObject(UObject* Asset)
	{
		if (!Asset) return false;
		UPackage* Pkg = Asset->GetOutermost();
		Pkg->MarkPackageDirty();
		const bool bMap = Asset->IsA<UWorld>();
		const FString File = FPackageName::LongPackageNameToFilename(Pkg->GetName(), bMap ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Pkg, Asset, *File, Args);
	}

	UMaterial* NewMaterial(const FString& Name)
	{
		const FString Object = FString::Printf(TEXT("%s/%s.%s"), MatPath, *Name, *Name);
		if (UMaterial* Existing = LoadObject<UMaterial>(nullptr, *Object, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			// wipe the graph so re-runs do not stack duplicate nodes
			UMaterialEditingLibrary::DeleteAllMaterialExpressions(Existing);
			return Existing;
		}
		return Cast<UMaterial>(Tools().CreateAsset(Name, MatPath, UMaterial::StaticClass(), NewObject<UMaterialFactoryNew>()));
	}

	template<typename T>
	T* Node(UMaterial* M, int32 X, int32 Y)
	{
		return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpression(M, T::StaticClass(), X, Y));
	}

	UMaterialExpression* Scalar(UMaterial* M, const TCHAR* Name, float V, int32 X, int32 Y)
	{
		UMaterialExpressionScalarParameter* P = Node<UMaterialExpressionScalarParameter>(M, X, Y);
		P->ParameterName = Name;
		P->DefaultValue = V;
		return P;
	}

	UMaterialExpression* Vector(UMaterial* M, const TCHAR* Name, const FLinearColor& V, int32 X, int32 Y)
	{
		UMaterialExpressionVectorParameter* P = Node<UMaterialExpressionVectorParameter>(M, X, Y);
		P->ParameterName = Name;
		P->DefaultValue = V;
		return P;
	}

	/** Tiny placeholder array asset: the default value of a texture-array parameter. Its colour
	 *  space and compression have to match the sampler type or the material will not compile. */
	UTexture2DArray* DefaultArray(const TCHAR* Name, bool bSRGB, TextureCompressionSettings Compression, FColor Fill)
	{
		const FString Package = FString::Printf(TEXT("/Game/Opus55Minecraft/Textures/%s"), Name);
		if (UTexture2DArray* Existing = LoadObject<UTexture2DArray>(nullptr, *FString::Printf(TEXT("%s.%s"), *Package, Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
			return Existing;
		UPackage* Pkg = CreatePackage(*Package);
		UTexture2DArray* T = NewObject<UTexture2DArray>(Pkg, Name, RF_Public | RF_Standalone);
		TArray<FColor> Px;
		Px.Init(Fill, 4 * 4 * 2);
		T->Source.Init(4, 4, 2, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Px.GetData()));
		T->SRGB = bSRGB;
		T->CompressionSettings = Compression;
		T->MipGenSettings = TMGS_NoMipmaps;
		T->PostEditChange();
		FAssetRegistryModule::AssetCreated(T);
		SaveObject(T);
		return T;
	}

	/** Texture-array parameter. The runtime supplies the real arrays through its MIDs; the default
	 *  only makes the generated HLSL declare a Texture2DArray with a matching sampler. */
	UMaterialExpression* TextureArray(UMaterial* M, const TCHAR* Name, EMaterialSamplerType Sampler, int32 X, int32 Y)
	{
		UMaterialExpressionTextureObjectParameter* P = Node<UMaterialExpressionTextureObjectParameter>(M, X, Y);
		P->ParameterName = Name;
		switch (Sampler)
		{
		case SAMPLERTYPE_Normal:
			P->Texture = DefaultArray(TEXT("T_MCArrayDefault_Normal"), false, TC_Normalmap, FColor(128, 128, 255, 255));
			break;
		case SAMPLERTYPE_LinearColor:
			P->Texture = DefaultArray(TEXT("T_MCArrayDefault_Linear"), false, TC_Default, FColor(255, 190, 0, 0));
			break;
		default:
			P->Texture = DefaultArray(TEXT("T_MCArrayDefault_Color"), true, TC_Default, FColor(200, 200, 200, 255));
			break;
		}
		P->SamplerType = Sampler;
		return P;
	}

	/** Frac(WorldPosition * Scale): a pattern coordinate that stays precise far from the origin
	 *  (the mesher emits geometry up to ~10 km out, where a raw world position loses precision). */
	UMaterialExpression* WorldPattern(UMaterial* M, float Scale, int32 X, int32 Y)
	{
		UMaterialExpressionWorldPosition* WP = Node<UMaterialExpressionWorldPosition>(M, X, Y);
		UMaterialExpressionMultiply* Mul = Node<UMaterialExpressionMultiply>(M, X + 200, Y);
		Mul->A.Connect(0, WP);
		Mul->ConstB = Scale;
		UMaterialExpressionFrac* F = Node<UMaterialExpressionFrac>(M, X + 380, Y);
		F->Input.Connect(0, Mul);
		return F;
	}

	/** Camera-relative world position: a plain float3, which is what a Custom node pin carries. */
	UMaterialExpression* CameraRelativePos(UMaterial* M, int32 X, int32 Y)
	{
		UMaterialExpressionWorldPosition* WP = Node<UMaterialExpressionWorldPosition>(M, X, Y);
		WP->WorldPositionShaderOffset = WPT_CameraRelative;
		return WP;
	}

	UMaterialExpression* TexCoord(UMaterial* M, int32 Index, int32 X, int32 Y)
	{
		UMaterialExpressionTextureCoordinate* T = Node<UMaterialExpressionTextureCoordinate>(M, X, Y);
		T->CoordinateIndex = Index;
		return T;
	}

	/** Component mask on a named output ("" = first output). Channels: any of "RGBA". */
	int32 OutputIndex(UMaterialExpression* E, const TCHAR* Name)
	{
		if (!Name || !*Name) return 0;
		const TArray<FExpressionOutput>& Outs = E->GetOutputs();
		for (int32 i = 0; i < Outs.Num(); ++i) if (Outs[i].OutputName == FName(Name)) return i;
		return 0;
	}

	UMaterialExpression* Mask(UMaterial* M, UMaterialExpression* Src, const TCHAR* SrcOutput, const TCHAR* Channels, int32 X, int32 Y)
	{
		UMaterialExpressionComponentMask* C = Node<UMaterialExpressionComponentMask>(M, X, Y);
		const FString Ch(Channels);
		C->R = Ch.Contains(TEXT("R"));
		C->G = Ch.Contains(TEXT("G"));
		C->B = Ch.Contains(TEXT("B"));
		C->A = Ch.Contains(TEXT("A"));
		C->Input.Connect(OutputIndex(Src, SrcOutput), Src);
		return C;
	}

	struct FPin { const TCHAR* Name; UMaterialExpression* Src; int32 Output; };

	/** Custom HLSL node. Outputs[0] is the "return" value; the rest are assigned by name in Code. */
	UMaterialExpressionCustom* Custom(UMaterial* M, const TCHAR* Desc, const FString& Code, ECustomMaterialOutputType ReturnType,
		const TArray<TPair<const TCHAR*, ECustomMaterialOutputType>>& Extra, const TArray<FPin>& Pins, int32 X, int32 Y)
	{
		UMaterialExpressionCustom* N = Node<UMaterialExpressionCustom>(M, X, Y);
		N->Description = Desc;
		N->Code = Code;
		N->OutputType = ReturnType;
		N->AdditionalOutputs.Reset();
		for (const TPair<const TCHAR*, ECustomMaterialOutputType>& O : Extra)
		{
			FCustomOutput Out;
			Out.OutputName = O.Key;
			Out.OutputType = O.Value;
			N->AdditionalOutputs.Add(Out);
		}
		N->RebuildOutputs();
		N->Inputs.Reset();
		for (const FPin& P : Pins)
		{
			FCustomInput In;
			In.InputName = P.Name;
			if (P.Src) In.Input.Connect(P.Output, P.Src);
			N->Inputs.Add(In);
		}
		return N;
	}

	void ToProperty(UMaterialExpression* Src, const TCHAR* Output, EMaterialProperty Prop)
	{
		UMaterialEditingLibrary::ConnectMaterialProperty(Src, Output, Prop);
	}

	void Finish(UMaterial* M)
	{
		// usages the runtime needs baked into the saved asset (a -game run cannot add them on the fly)
		bool bRecompile = false;
		M->SetMaterialUsage(bRecompile, MATUSAGE_Nanite);
		UMaterialEditingLibrary::LayoutMaterialExpressions(M);
		UMaterialEditingLibrary::RecompileMaterial(M);
		SaveObject(M);
	}

	// ------------------------------------------------------------------ HLSL

	// Terrain layers. Vertex streams (FMCMeshVertex): TexCoord0 = tile UV, TexCoord1 = (array slice,
	// flags), TexCoord2 = (sky light, block light) in 0..1, VertexColor = tint (rgb) + AO (a).
	// Lighting model: the sun, moon, sky light and Lumen light the surface physically; the voxel
	// sky light scales the ambient term (AO) so caves go dark, and voxel block light is emitted so
	// torches glow and feed Lumen's GI.
	const TCHAR* VoxelCode = TEXT(R"(
float3 tx = float3(InUV, Slice);
float4 albedo = ProcessMaterialColorTextureLookup(AlbedoTex.SampleGrad(AlbedoTexSampler, tx, ddx(InUV), ddy(InUV)));
float4 orme = ProcessMaterialLinearColorTextureLookup(OrmeTex.SampleGrad(OrmeTexSampler, tx, ddx(InUV), ddy(InUV)));
float2 nxy = NormalTex.SampleGrad(NormalTexSampler, tx, ddx(InUV), ddy(InUV)).rg * 2.0 - 1.0;
OutNormal = float3(nxy, sqrt(saturate(1.0 - dot(nxy, nxy))));

// vertex flags (MCRender::EVertexFlags)
float isEmissive = (fmod(Flags, 4.0) >= 2.0) ? 1.0 : 0.0;
float isParticle = (fmod(Flags, 64.0) >= 32.0) ? 1.0 : 0.0;

float item = saturate(ItemMode);
float sky = lerp(saturate(EnvLight.x), 1.0, item);
float blk = saturate(EnvLight.y) * (1.0 - item);

// vertex colours reach the shader as their raw bytes, i.e. still sRGB-encoded (biome colormaps, dyes, particle
// colours): decode the tint to linear before it multiplies the albedo; alpha carries linear AO and stays raw
float3 vtint = VColor.rgb * (VColor.rgb * (VColor.rgb * 0.305306011 + 0.682171111) + 0.012522878);
// opaque textures keep their tint mask in alpha (the grass fringe over dirt, bed planks stay wood); cutout and
// translucent textures use alpha for opacity and are tinted wherever they are visible
vtint = lerp(float3(1.0, 1.0, 1.0), vtint, lerp(1.0, albedo.a, TintFromAlpha));
float3 base = albedo.rgb * vtint;
float vao = lerp(saturate(VColor.a), 1.0, item);
OutAO = saturate(vao * lerp(0.18, 1.0, sky) * lerp(1.0, orme.r, 0.75));

// torch / lava / glowstone light: warm, grows with the square of the level
float torch = blk * blk;
float3 emissive = base * torch * float3(1.00, 0.70, 0.42) * TorchStrength;
emissive += base * (AmbientFloor * 0.35 + NightVision * 0.45);
emissive += base * max(orme.a, isEmissive * 0.9) * EmissiveStrength;
// leaves, grass and flowers: light scattered through the blades keeps shaded foliage green instead of black
float isFoliage = saturate(((fmod(Flags, 16.0) >= 8.0) ? 1.0 : 0.0) + ((fmod(Flags, 8.0) >= 4.0) ? 1.0 : 0.0));
emissive += base * isFoliage * 0.28 * sky * saturate(SkyBrightness) * (1.0 - item);

float rough = orme.g;
rough = lerp(rough, rough * 0.35, saturate(Wetness) * sky);
OutRoughness = clamp(rough, 0.04, 1.0);
OutMetallic = orme.b;
OutOpacity = albedo.a;

// particles and other emitted sprites stay unlit
if (isParticle > 0.5)
{
    emissive = base * 1.6;
    OutOpacity = albedo.a * saturate(VColor.a);
    base = float3(0.0, 0.0, 0.0);
    OutRoughness = 1.0;
}
OutEmissive = emissive * LayerEmissive + base * LayerGlow;
return base * LayerBase;
)");

	// Vegetation sway (VF_Wave = 4): world-space wobble, larger at the top of the model.
	const TCHAR* WaveCode = TEXT(R"(
float isWave = (fmod(Flags, 8.0) >= 4.0) ? 1.0 : 0.0;
float t = Time;
float3 p = Pattern * 6.2831853;   // frac(world / 10 m), scaled to one period
float3 o = float3(sin(t * 1.7 + p.x * 3.0 + p.y * 2.0), cos(t * 1.3 + p.y * 2.6 + p.x * 1.3), 0.0);
return o * isWave * SwayCm;
)");

	// Nether portal / end gateway: swirling self-lit texture.
	const TCHAR* EndPortalCode = TEXT(R"(
// parallax star field, layered at several depths (screen-independent: uses world position)
float3 wp = Pattern * 10.0;       // wrapped world position in blocks
float3 col = float3(0.02, 0.05, 0.06);
for (int i = 1; i <= 5; i++)
{
    float s = 3.0 + i * 2.5;
    float2 cell = floor(wp.xy * s + float2(Time * 0.02 * i, Time * 0.013 * i) + wp.z * 0.3);
    float h = frac(sin(dot(cell, float2(12.9898, 78.233)) + i * 17.0) * 43758.5453);
    float star = step(0.93, h) * (0.5 + 0.5 * sin(Time * 2.0 + h * 40.0));
    float3 tint = lerp(float3(0.20, 0.85, 0.75), float3(0.55, 0.30, 0.95), frac(h * 7.0));
    col += tint * star * (1.2 / i);
}
return col;
)");

	// Rig parts (Tools/BlenderMCP/build_assets.py): vertex rgb = albedo, alpha 1 = the mob tint
	// applies (variants, dyes), 0.5 = self-lit texel (eyes, cracks), 0 = fixed colour.
	// Meshes without colours (the engine cube fallback) read white / alpha 1 = fully tinted.
	const TCHAR* EntityCode = TEXT(R"(
float a = VCol.a;
float tintMask = step(0.75, a);
float glowMask = step(0.25, a) * (1.0 - step(0.75, a));
// Tools/BlenderMCP exports linear colours (colors_type LINEAR); the FBX importer treats FBX colours as sRGB and the
// mesh builder re-encodes them, so the vertex buffer ends up holding exactly the exported linear values: use them as is
float3 albedo = VCol.rgb * VertexColorScale;
float3 base = albedo * lerp(float3(1.0, 1.0, 1.0), Tint.rgb, tintMask);
float hurt = saturate(Hurt);
base = lerp(base, float3(1.0, 0.22, 0.22) * (0.35 + 0.65 * dot(base, float3(0.33, 0.33, 0.34))), hurt * 0.6);
OutEmissive = base * saturate(Emissive + Glow * 0.8) * 2.0 + base * glowMask * 3.0 + float3(0.35, 0.0, 0.0) * hurt;
// local light at the entity (set by the game from the voxel light): soft sky fill so shaded sides are not black,
// warm torch light that matches the terrain's block light
OutEmissive += base * Fill + base * TorchLight * float3(1.0, 0.70, 0.42);
return base;
)");

	const TCHAR* GlowCode = TEXT(R"(
float pulse = 1.0 + 0.22 * sin(Time * 6.0);
float strength = max(max(Emissive, Glow), 0.25);
return Color.rgb * strength * pulse;
)");

	const TCHAR* BeamCode = TEXT(R"(
float3 v = -normalize(RelPos);
float rim = pow(saturate(abs(dot(v, normalize(NormalWS)))), 1.5);
float band = 0.70 + 0.30 * sin(UV.y * 18.0 - Time * 4.0);
OutOpacity = saturate(rim * band * 0.85);
return Color.rgb * max(max(Emissive, Glow), 0.5) * band;
)");

	const TCHAR* SkyCode = TEXT(R"(
float3 dir = normalize(RelPos);
float isNether = step(0.5, Dimension) * (1.0 - step(1.5, Dimension));
float isEnd = step(1.5, Dimension);

// overworld: the SkyAtmosphere draws the day sky, this dome adds stars and the moon at night
float3 sd = floor(dir * 280.0);
float hs = frac(sin(dot(sd, float3(12.9898, 78.233, 37.719))) * 43758.5453);
float star = step(0.9975, hs) * (0.6 + 0.4 * sin(Time * 1.5 + hs * 60.0)) * saturate(dir.z * 3.0);
float3 over = float3(1.0, 0.98, 0.92) * star * 3.0 * Night * (1.0 - saturate(Rain));
OutOpacity = saturate(star * Night * (1.0 - Rain));

// nether: thick crimson haze, brighter near the horizon
float3 nether = lerp(float3(0.20, 0.03, 0.02), float3(0.42, 0.10, 0.05), pow(1.0 - abs(dir.z), 2.0));

// the End: dark violet void with drifting specks
float3 ec = floor(dir * 170.0 + float3(0.0, 0.0, Time * 0.01));
float he = frac(sin(dot(ec, float3(12.9898, 78.233, 37.719))) * 43758.5453);
float neb = 0.5 + 0.5 * sin(dir.x * 5.0 + Time * 0.04) * sin(dir.y * 4.0 - Time * 0.03);
float3 endc = float3(0.03, 0.015, 0.05) + float3(0.10, 0.05, 0.16) * neb * 0.35 + float3(0.8, 0.7, 1.0) * step(0.994, he);

OutOpacity = lerp(OutOpacity, 1.0, saturate(isNether + isEnd));
return lerp(lerp(over, nether, isNether), endc, isEnd);
)");

	const TCHAR* CloudCode = TEXT(R"(
float2 p = UV * CloudScale + float2(Time * 0.010, Time * 0.004);
float n = 0.0;
float amp = 0.5;
for (int i = 0; i < 5; i++)
{
    n += amp * (sin(p.x * 3.1 + cos(p.y * 2.3) * 1.7) * 0.5 + cos(p.y * 2.7 + sin(p.x * 1.9)) * 0.5);
    p *= 2.03;
    amp *= 0.5;
}
float cover = saturate(smoothstep(0.10, 0.62, n * 0.5 + 0.5) * CloudCover);
OutOpacity = cover * Fade * (1.0 - Rain * 0.5);
return lerp(float3(0.55, 0.60, 0.72), float3(1.0, 0.99, 0.96), 1.0 - Night * 0.85);
)");
}

// ---------------------------------------------------------------------------------
// Terrain layer materials

UMaterial* UMCEditorLibrary::BuildVoxelMaterial(EMCOpus55Layer Layer, bool bItemVariant)
{
	using namespace MCContent;
	const int32 L = (int32)Layer;
	// FString::Printf needs a literal format string, so pick the prefix first
	const FString Name = bItemVariant
		? FString::Printf(TEXT("M_MCItemVoxel_%s"), LayerNames[L])
		: FString::Printf(TEXT("M_MCVoxel_%s"), LayerNames[L]);
	UMaterial* M = NewMaterial(Name);
	if (!M) return nullptr;

	// ---- blend / shading
	float LayerBase = 1.f, LayerEmissive = 1.f, LayerGlow = 0.f;
	switch (Layer)
	{
	case EMCOpus55Layer::Opaque:
		M->BlendMode = BLEND_Opaque;
		break;
	case EMCOpus55Layer::Cutout:
		M->BlendMode = BLEND_Masked;
		M->TwoSided = true;
		M->OpacityMaskClipValue = 0.5f;
		break;
	case EMCOpus55Layer::Translucent:
		M->BlendMode = BLEND_Translucent;
		M->TwoSided = true;
		M->TranslucencyLightingMode = TLM_SurfacePerPixelLighting;
		break;
	case EMCOpus55Layer::Water:
		M->BlendMode = BLEND_Translucent;
		M->TwoSided = true;
		M->TranslucencyLightingMode = TLM_SurfacePerPixelLighting;
		break;
	case EMCOpus55Layer::Lava:
		M->BlendMode = BLEND_Opaque;
		LayerBase = 0.35f; LayerGlow = 2.4f;
		break;
	case EMCOpus55Layer::Portal:
		M->BlendMode = BLEND_Translucent;
		M->TwoSided = true;
		M->TranslucencyLightingMode = TLM_VolumetricNonDirectional;
		LayerBase = 0.2f; LayerGlow = 1.6f;
		break;
	case EMCOpus55Layer::EndPortal:
		M->BlendMode = BLEND_Opaque;
		M->SetShadingModel(MSM_Unlit);
		break;
	}

	// ---- parameters the runtime sets on the MIDs
	UMaterialExpression* PSky = Scalar(M, TEXT("SkyBrightness"), 1.f, -2400, -800);
	UMaterialExpression* PAmb = Scalar(M, TEXT("AmbientFloor"), 0.05f, -2400, -720);
	UMaterialExpression* PNight = Scalar(M, TEXT("NightVision"), 0.f, -2400, -640);
	UMaterialExpression* PWet = Scalar(M, TEXT("Wetness"), 0.f, -2400, -560);
	UMaterialExpression* PTime = Scalar(M, TEXT("GameTime"), 0.f, -2400, -480);
	UMaterialExpression* PItem = Scalar(M, TEXT("ItemMode"), bItemVariant ? 1.f : 0.f, -2400, -400);
	UMaterialExpression* PTorch = Scalar(M, TEXT("TorchStrength"), 2.2f, -2400, -320);
	UMaterialExpression* PEmis = Scalar(M, TEXT("EmissiveStrength"), 2.5f, -2400, -240);
	Scalar(M, TEXT("LayerCount"), 1.f, -2400, -160);          // written by the renderer, informative only
	Vector(M, TEXT("FogColor"), FLinearColor(0.7f, 0.8f, 1.f), -2400, -80);
	UMaterialExpression* TexA = TextureArray(M, TEXT("Albedo"), SAMPLERTYPE_Color, -2400, 20);
	UMaterialExpression* TexN = TextureArray(M, TEXT("Normal"), SAMPLERTYPE_Normal, -2400, 180);
	UMaterialExpression* TexO = TextureArray(M, TEXT("ORME"), SAMPLERTYPE_LinearColor, -2400, 340);

	// ---- vertex streams
	UMaterialExpression* UV0 = TexCoord(M, 0, -2400, 500);
	UMaterialExpression* UV1 = TexCoord(M, 1, -2400, 580);
	UMaterialExpression* UV2 = TexCoord(M, 2, -2400, 660);
	UMaterialExpression* VCol = Node<UMaterialExpressionVertexColor>(M, -2400, 740);
	UMaterialExpression* Slice = Mask(M, UV1, TEXT(""), TEXT("R"), -2150, 580);
	UMaterialExpression* Flags = Mask(M, UV1, TEXT(""), TEXT("G"), -2150, 640);
	UMaterialExpression* VRGBA = Node<UMaterialExpressionVertexColor>(M, -2150, 740);

	// vertex colour is needed as float4: output index 0 = RGB, 4 = A; pass both through a
	// tiny custom node so the main HLSL gets one float4
	UMaterialExpressionCustom* VC4 = Custom(M, TEXT("VertexRGBA"), TEXT("return float4(RGB, A);"), CMOT_Float4, {},
		{ { TEXT("RGB"), VCol, 0 }, { TEXT("A"), VRGBA, 4 } }, -1900, 740);

	UMaterialExpressionCustom* N = Custom(M, TEXT("Opus55Voxel"), VoxelCode, CMOT_Float3,
		{ { TEXT("OutNormal"), CMOT_Float3 }, { TEXT("OutEmissive"), CMOT_Float3 }, { TEXT("OutRoughness"), CMOT_Float1 },
		  { TEXT("OutMetallic"), CMOT_Float1 }, { TEXT("OutOpacity"), CMOT_Float1 }, { TEXT("OutAO"), CMOT_Float1 } },
		{ { TEXT("InUV"), UV0, 0 }, { TEXT("Slice"), Slice, 0 }, { TEXT("Flags"), Flags, 0 }, { TEXT("EnvLight"), UV2, 0 },
		  { TEXT("VColor"), VC4, 0 }, { TEXT("ItemMode"), PItem, 0 }, { TEXT("AmbientFloor"), PAmb, 0 },
		  { TEXT("NightVision"), PNight, 0 }, { TEXT("Wetness"), PWet, 0 }, { TEXT("TorchStrength"), PTorch, 0 },
		  { TEXT("EmissiveStrength"), PEmis, 0 }, { TEXT("SkyBrightness"), PSky, 0 },
		  { TEXT("LayerBase"), Scalar(M, TEXT("LayerBase"), LayerBase, -1900, 820), 0 },
		  { TEXT("LayerEmissive"), Scalar(M, TEXT("LayerEmissive"), LayerEmissive, -1900, 900), 0 },
		  { TEXT("LayerGlow"), Scalar(M, TEXT("LayerGlow"), LayerGlow, -1900, 980), 0 },
		  { TEXT("TintFromAlpha"), Scalar(M, TEXT("TintFromAlpha"), Layer == EMCOpus55Layer::Opaque ? 1.f : 0.f, -1900, 1040), 0 },
		  { TEXT("AlbedoTex"), TexA, 0 }, { TEXT("NormalTex"), TexN, 0 }, { TEXT("OrmeTex"), TexO, 0 } },
		-1500, 0);

	if (Layer == EMCOpus55Layer::EndPortal)
	{
		UMaterialExpression* WP = WorldPattern(M, 0.001f, -1900, 1100);
		UMaterialExpression* T = Node<UMaterialExpressionTime>(M, -1900, 1180);
		UMaterialExpressionCustom* Stars = Custom(M, TEXT("EndPortalStars"), EndPortalCode, CMOT_Float3, {},
			{ { TEXT("Pattern"), WP, 0 }, { TEXT("Time"), T, 0 } }, -1500, 600);
		ToProperty(Stars, TEXT(""), MP_EmissiveColor);
		Finish(M);
		return M;
	}

	ToProperty(N, TEXT("return"), MP_BaseColor);
	ToProperty(N, TEXT("OutEmissive"), MP_EmissiveColor);
	ToProperty(N, TEXT("OutNormal"), MP_Normal);
	ToProperty(N, TEXT("OutRoughness"), MP_Roughness);
	ToProperty(N, TEXT("OutMetallic"), MP_Metallic);
	ToProperty(N, TEXT("OutAO"), MP_AmbientOcclusion);
	if (M->BlendMode == BLEND_Masked) ToProperty(N, TEXT("OutOpacity"), MP_OpacityMask);
	if (M->BlendMode == BLEND_Translucent)
	{
		if (Layer == EMCOpus55Layer::Water)
		{
			// water: fairly clear up close, the texture alpha adds foam / depth variation
			UMaterialExpressionCustom* WaterA = Custom(M, TEXT("WaterOpacity"), TEXT("return saturate(0.58 + A * 0.32);"), CMOT_Float1, {},
				{ { TEXT("A"), N, 5 } }, -1100, 400);
			ToProperty(WaterA, TEXT(""), MP_Opacity);
		}
		else ToProperty(N, TEXT("OutOpacity"), MP_Opacity);
	}

	// vegetation sway on the cutout layer (leaves, grass, flowers) and on items: none
	if (Layer == EMCOpus55Layer::Cutout && !bItemVariant)
	{
		UMaterialExpression* WP = WorldPattern(M, 0.001f, -1900, 1100);
		UMaterialExpression* T = Node<UMaterialExpressionTime>(M, -1900, 1180);
		UMaterialExpressionCustom* Wave = Custom(M, TEXT("Sway"), WaveCode, CMOT_Float3, {},
			{ { TEXT("Flags"), Flags, 0 }, { TEXT("Pattern"), WP, 0 }, { TEXT("Time"), T, 0 },
			  { TEXT("SwayCm"), Scalar(M, TEXT("SwayCm"), 1.6f, -1900, 1260), 0 } }, -1500, 800);
		ToProperty(Wave, TEXT(""), MP_WorldPositionOffset);
	}
	(void)PTime;
	Finish(M);
	return M;
}

// ---------------------------------------------------------------------------------
// Entity / effect / sky materials

UMaterial* UMCEditorLibrary::BuildEntityMaterial()
{
	using namespace MCContent;
	UMaterial* M = NewMaterial(TEXT("M_MCEntity"));
	if (!M) return nullptr;
	M->BlendMode = BLEND_Opaque;
	M->SetShadingModel(MSM_DefaultLit);

	UMaterialExpression* Tint = Vector(M, TEXT("Tint"), FLinearColor::White, -1400, -200);
	Vector(M, TEXT("Color"), FLinearColor::White, -1400, -120);                  // set together with Tint by MakeMID
	UMaterialExpression* Hurt = Scalar(M, TEXT("Hurt"), 0.f, -1400, -40);
	UMaterialExpression* Glow = Scalar(M, TEXT("Glow"), 0.f, -1400, 40);
	UMaterialExpression* Emis = Scalar(M, TEXT("Emissive"), 0.f, -1400, 120);
	Scalar(M, TEXT("Part"), 0.f, -1400, 200);
	UMaterialExpression* VRGB = Node<UMaterialExpressionVertexColor>(M, -1400, 280);
	UMaterialExpression* VA = Node<UMaterialExpressionVertexColor>(M, -1400, 360);
	UMaterialExpressionCustom* VC4 = Custom(M, TEXT("VertexRGBA"), TEXT("return float4(RGB, A);"), CMOT_Float4, {},
		{ { TEXT("RGB"), VRGB, 0 }, { TEXT("A"), VA, 4 } }, -1100, 300);

	UMaterialExpression* VScale = Scalar(M, TEXT("VertexColorScale"), 1.f, -1400, 440);
	UMaterialExpressionCustom* N = Custom(M, TEXT("Opus55Entity"), EntityCode, CMOT_Float3,
		{ { TEXT("OutEmissive"), CMOT_Float3 } },
		{ { TEXT("VCol"), VC4, 0 }, { TEXT("Tint"), Tint, 0 }, { TEXT("Hurt"), Hurt, 0 }, { TEXT("Glow"), Glow, 0 },
		  { TEXT("Emissive"), Emis, 0 }, { TEXT("VertexColorScale"), VScale, 0 },
		  { TEXT("Fill"), Scalar(M, TEXT("Fill"), 0.1f, -1400, 520), 0 },
		  { TEXT("TorchLight"), Scalar(M, TEXT("TorchLight"), 0.f, -1400, 600), 0 } },
		-800, 0);
	ToProperty(N, TEXT("return"), MP_BaseColor);
	ToProperty(N, TEXT("OutEmissive"), MP_EmissiveColor);
	ToProperty(Scalar(M, TEXT("Roughness"), 0.62f, -800, 300), TEXT(""), MP_Roughness);
	// tool heads, armour and golem plating set this per material slot (the runtime reads slot "MI_Metal")
	ToProperty(Scalar(M, TEXT("Metallic"), 0.f, -800, 380), TEXT(""), MP_Metallic);
	Finish(M);
	return M;
}

UMaterial* UMCEditorLibrary::BuildGlowMaterial()
{
	using namespace MCContent;
	UMaterial* M = NewMaterial(TEXT("M_MCGlow"));
	if (!M) return nullptr;
	M->BlendMode = BLEND_Opaque;
	M->SetShadingModel(MSM_Unlit);
	M->TwoSided = true;
	UMaterialExpression* Color = Vector(M, TEXT("Color"), FLinearColor::White, -1200, -100);
	Vector(M, TEXT("Tint"), FLinearColor::White, -1200, -20);
	UMaterialExpression* Emis = Scalar(M, TEXT("Emissive"), 1.f, -1200, 60);
	UMaterialExpression* Glow = Scalar(M, TEXT("Glow"), 1.f, -1200, 140);
	UMaterialExpression* T = Node<UMaterialExpressionTime>(M, -1200, 220);
	UMaterialExpressionCustom* N = Custom(M, TEXT("Opus55Glow"), GlowCode, CMOT_Float3, {},
		{ { TEXT("Color"), Color, 0 }, { TEXT("Emissive"), Emis, 0 }, { TEXT("Glow"), Glow, 0 }, { TEXT("Time"), T, 0 } }, -800, 0);
	ToProperty(N, TEXT(""), MP_EmissiveColor);
	Finish(M);
	return M;
}

UMaterial* UMCEditorLibrary::BuildBeamMaterial()
{
	using namespace MCContent;
	UMaterial* M = NewMaterial(TEXT("M_MCBeam"));
	if (!M) return nullptr;
	M->BlendMode = BLEND_Translucent;
	M->SetShadingModel(MSM_Unlit);
	M->TwoSided = true;
	UMaterialExpression* Color = Vector(M, TEXT("Color"), FLinearColor::White, -1400, -100);
	Vector(M, TEXT("Tint"), FLinearColor::White, -1400, -20);
	UMaterialExpression* Emis = Scalar(M, TEXT("Emissive"), 1.f, -1400, 60);
	UMaterialExpression* Glow = Scalar(M, TEXT("Glow"), 1.f, -1400, 140);
	UMaterialExpression* T = Node<UMaterialExpressionTime>(M, -1400, 220);
	UMaterialExpression* UV = TexCoord(M, 0, -1400, 300);
	UMaterialExpression* NWS = Node<UMaterialExpressionVertexNormalWS>(M, -1400, 380);
	UMaterialExpression* RP = CameraRelativePos(M, -1400, 460);
	UMaterialExpressionCustom* N = Custom(M, TEXT("Opus55Beam"), BeamCode, CMOT_Float3, { { TEXT("OutOpacity"), CMOT_Float1 } },
		{ { TEXT("Color"), Color, 0 }, { TEXT("Emissive"), Emis, 0 }, { TEXT("Glow"), Glow, 0 }, { TEXT("Time"), T, 0 },
		  { TEXT("UV"), UV, 0 }, { TEXT("NormalWS"), NWS, 0 }, { TEXT("RelPos"), RP, 0 } }, -900, 0);
	ToProperty(N, TEXT("return"), MP_EmissiveColor);
	ToProperty(N, TEXT("OutOpacity"), MP_Opacity);
	Finish(M);
	return M;
}

UMaterial* UMCEditorLibrary::BuildSkyMaterial()
{
	using namespace MCContent;
	UMaterial* M = NewMaterial(TEXT("M_MCSky"));
	if (!M) return nullptr;
	// translucent unlit dome: stars over the atmosphere in the Overworld, full sky elsewhere
	M->BlendMode = BLEND_Translucent;
	M->SetShadingModel(MSM_Unlit);
	M->TwoSided = true;
	M->bUseTranslucencyVertexFog = false;
	UMaterialExpression* Night = Scalar(M, TEXT("Night"), 0.f, -1400, -200);
	UMaterialExpression* Dim = Scalar(M, TEXT("Dimension"), 0.f, -1400, -120);
	UMaterialExpression* Rain = Scalar(M, TEXT("Rain"), 0.f, -1400, -40);
	UMaterialExpression* Time = Scalar(M, TEXT("Time"), 0.f, -1400, 40);
	UMaterialExpression* RP = CameraRelativePos(M, -1400, 120);
	UMaterialExpressionCustom* N = Custom(M, TEXT("Opus55Sky"), SkyCode, CMOT_Float3, { { TEXT("OutOpacity"), CMOT_Float1 } },
		{ { TEXT("Night"), Night, 0 }, { TEXT("Dimension"), Dim, 0 }, { TEXT("Rain"), Rain, 0 }, { TEXT("Time"), Time, 0 },
		  { TEXT("RelPos"), RP, 0 } }, -900, 0);
	ToProperty(N, TEXT("return"), MP_EmissiveColor);
	ToProperty(N, TEXT("OutOpacity"), MP_Opacity);
	Finish(M);
	return M;
}

UMaterial* UMCEditorLibrary::BuildCloudMaterial()
{
	using namespace MCContent;
	UMaterial* M = NewMaterial(TEXT("M_MCCloudLayer"));
	if (!M) return nullptr;
	// a flat cloud-layer material for low settings; volumetric clouds use the engine's cloud material
	M->BlendMode = BLEND_Translucent;
	M->SetShadingModel(MSM_Unlit);
	M->TwoSided = true;
	UMaterialExpression* Time = Scalar(M, TEXT("Time"), 0.f, -1200, -160);
	UMaterialExpression* Cover = Scalar(M, TEXT("CloudCover"), 0.55f, -1200, -80);
	UMaterialExpression* Scale = Scalar(M, TEXT("CloudScale"), 2.f, -1200, 0);
	UMaterialExpression* Night = Scalar(M, TEXT("Night"), 0.f, -1200, 80);
	UMaterialExpression* Rain = Scalar(M, TEXT("Rain"), 0.f, -1200, 160);
	UMaterialExpression* Fade = Scalar(M, TEXT("Fade"), 1.f, -1200, 240);
	UMaterialExpression* UV = TexCoord(M, 0, -1200, 320);
	UMaterialExpressionCustom* N = Custom(M, TEXT("Opus55Clouds"), CloudCode, CMOT_Float3, { { TEXT("OutOpacity"), CMOT_Float1 } },
		{ { TEXT("Time"), Time, 0 }, { TEXT("CloudCover"), Cover, 0 }, { TEXT("CloudScale"), Scale, 0 }, { TEXT("Night"), Night, 0 },
		  { TEXT("Rain"), Rain, 0 }, { TEXT("Fade"), Fade, 0 }, { TEXT("UV"), UV, 0 } }, -800, 0);
	ToProperty(N, TEXT("return"), MP_EmissiveColor);
	ToProperty(N, TEXT("OutOpacity"), MP_Opacity);
	Finish(M);
	return M;
}

// ---------------------------------------------------------------------------------
// Default map

bool UMCEditorLibrary::BuildDefaultMap()
{
	using namespace MCContent;
	const FString Package = FString::Printf(TEXT("%s/%s"), MapPath, MapName);
	UWorld* World = LoadObject<UWorld>(nullptr, *FString::Printf(TEXT("%s.%s"), *Package, MapName), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!World)
	{
		UWorldFactory* Factory = NewObject<UWorldFactory>();
		Factory->WorldType = EWorldType::Inactive;
		Factory->bInformEngineOfWorld = true;
		Factory->bCreateWorldPartition = false;
		World = Cast<UWorld>(Tools().CreateAsset(MapName, MapPath, UWorld::StaticClass(), Factory));
	}
	if (!World) return false;
	// the game spawns its own sky, lights, terrain and player: the level only fixes the game mode
	if (AWorldSettings* WS = World->GetWorldSettings())
	{
		WS->DefaultGameMode = AMCGameMode::StaticClass();
		WS->bEnableWorldBoundsChecks = false;
		WS->KillZ = -1000000.f;
		WS->MarkPackageDirty();
	}
	return SaveObject(World);
}

// ---------------------------------------------------------------------------------
// Hero meshes (Blender -> FBX -> /Game)

int32 UMCEditorLibrary::ImportHeroMeshes()
{
	const FString Root = FPaths::ProjectSavedDir() / TEXT("Opus55Fbx");
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.fbx"), true, false);
	if (Files.Num() == 0)
	{
		UE_LOG(LogOpus55Editor, Warning, TEXT("No FBX files under %s - run Tools/BlenderMCP/build_assets.py through the blender_unreal MCP first."), *Root);
		return 0;
	}

	TArray<UAssetImportTask*> Tasks;
	for (const FString& File : Files)
	{
		// Saved/Opus55Fbx/<Category>/<Rig>/SM_x.fbx -> /Game/Opus55Minecraft/<Category>/<Rig>
		FString Rel = File;
		FPaths::MakePathRelativeTo(Rel, *(Root + TEXT("/")));
		FString Dir = FPaths::GetPath(Rel);
		Dir.ReplaceInline(TEXT("\\"), TEXT("/"));
		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = File;
		Task->DestinationPath = TEXT("/Game/Opus55Minecraft/") + Dir;
		Task->DestinationName = FPaths::GetBaseFilename(File);
		Task->bAutomated = true;
		Task->bReplaceExisting = true;
		Task->bReplaceExistingSettings = true;
		Task->bSave = false;
		// geometry + vertex colours + material slot names only: the runtime assigns M_MCEntity instances
		UFbxImportUI* Options = NewObject<UFbxImportUI>();
		Options->bImportMesh = true;
		Options->bImportAsSkeletal = false;
		Options->bImportMaterials = false;
		Options->bImportTextures = false;
		Options->bImportAnimations = false;
		Options->StaticMeshImportData->bCombineMeshes = true;
		Options->StaticMeshImportData->VertexColorImportOption = EVertexColorImportOption::Replace;
		Options->StaticMeshImportData->bGenerateLightmapUVs = false;
		Options->StaticMeshImportData->bAutoGenerateCollision = false;
		Task->Options = Options;
		Tasks.Add(Task);
	}
	UE_LOG(LogOpus55Editor, Display, TEXT("Importing %d hero meshes..."), Tasks.Num());
	MCContent::Tools().ImportAssetTasks(Tasks);

	// post-process: game-ready defaults (no collision, entity material, no Nanite for tiny parts)
	UMaterialInterface* Entity = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Opus55Minecraft/Materials/M_MCEntity.M_MCEntity"));
	int32 Imported = 0;
	for (UAssetImportTask* Task : Tasks)
	{
		for (UObject* Obj : Task->GetObjects())
		{
			UStaticMesh* SM = Cast<UStaticMesh>(Obj);
			if (!SM) continue;
			SM->Modify();
			if (Entity)
			{
				TArray<FStaticMaterial> Mats = SM->GetStaticMaterials();
				if (Mats.Num() == 0) Mats.Add(FStaticMaterial(Entity, TEXT("Entity")));
				for (FStaticMaterial& S : Mats) S.MaterialInterface = Entity;
				SM->SetStaticMaterials(Mats);
			}
			SM->bAllowCPUAccess = false;
			// rig parts and props are small movable meshes: classic rendering beats Nanite's per-instance cost
			SM->NaniteSettings.bEnabled = false;
			SM->PostEditChange();
			MCContent::SaveObject(SM);
			++Imported;
		}
	}
	UE_LOG(LogOpus55Editor, Display, TEXT("Imported %d static meshes."), Imported);
	return Imported;
}

// ---------------------------------------------------------------------------------
// Verification

TArray<FString> UMCEditorLibrary::RequiredMaterialPaths()
{
	TArray<FString> Out;
	for (const TCHAR* L : MCContent::LayerNames) Out.Add(FString::Printf(TEXT("/Game/Opus55Minecraft/Materials/M_MCVoxel_%s.M_MCVoxel_%s"), L, L));
	for (const TCHAR* Name : { TEXT("M_MCEntity"), TEXT("M_MCGlow"), TEXT("M_MCBeam"), TEXT("M_MCSky") })
		Out.Add(FString::Printf(TEXT("/Game/Opus55Minecraft/Materials/%s.%s"), Name, Name));
	return Out;
}

TArray<FString> UMCEditorLibrary::RequiredMobRigNames()
{
	TArray<FString> Out;
	for (const FMCRigDef& R : MCRigs::All()) Out.Add(R.Id.ToString());
	return Out;
}

FString UMCEditorLibrary::VerifyContent()
{
	FString Report;
	int32 Missing = 0, Found = 0;
	for (const FString& P : RequiredMaterialPaths())
	{
		const bool bOk = LoadObject<UMaterialInterface>(nullptr, *P, nullptr, LOAD_NoWarn | LOAD_Quiet) != nullptr;
		Report += FString::Printf(TEXT("%s %s\n"), bOk ? TEXT("[ok]  ") : TEXT("[MISS]"), *P);
		bOk ? ++Found : ++Missing;
	}
	const FString Map = FString::Printf(TEXT("%s/%s.%s"), MCContent::MapPath, MCContent::MapName, MCContent::MapName);
	const bool bMap = LoadObject<UWorld>(nullptr, *Map, nullptr, LOAD_NoWarn | LOAD_Quiet) != nullptr;
	Report += FString::Printf(TEXT("%s %s\n"), bMap ? TEXT("[ok]  ") : TEXT("[MISS]"), *Map);
	bMap ? ++Found : ++Missing;

	// rig parts: count how many resolve to an authored mesh (the rest use the box fallback)
	int32 PartsTotal = 0, PartsAuthored = 0, RigsFull = 0;
	for (const FMCRigDef& R : MCRigs::All())
	{
		int32 Authored = 0;
		for (const FMCRigPart& P : R.Parts)
		{
			++PartsTotal;
			if (LoadObject<UStaticMesh>(nullptr, *MCRigs::PartMeshPath(R.Id, P.Name), nullptr, LOAD_NoWarn | LOAD_Quiet)) { ++PartsAuthored; ++Authored; }
		}
		if (Authored == R.Parts.Num()) ++RigsFull;
	}
	Report += FString::Printf(TEXT("rig parts authored: %d / %d (%d rigs complete of %d)\n"), PartsAuthored, PartsTotal, RigsFull, MCRigs::All().Num());
	Report += FString::Printf(TEXT("required assets: %d ok, %d missing\n"), Found, Missing);
	return Report;
}

FString UMCEditorLibrary::ProbeMeshColors(const FString& MeshPath)
{
	UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	if (!SM) return TEXT("missing ") + MeshPath;
	FString Out = MeshPath;
	if (const FMeshDescription* Desc = SM->GetMeshDescription(0))
	{
		TVertexInstanceAttributesConstRef<FVector4f> Colors = Desc->VertexInstanceAttributes().GetAttributesRef<FVector4f>(MeshAttribute::VertexInstance::Color);
		if (Colors.IsValid() && Desc->VertexInstances().Num() > 0)
		{
			const FVector4f C = Colors[FVertexInstanceID(0)];
			Out += FString::Printf(TEXT(" description=(%.3f %.3f %.3f %.3f)"), C.X, C.Y, C.Z, C.W);
		}
	}
	if (const FStaticMeshRenderData* RD = SM->GetRenderData())
	{
		if (RD->LODResources.Num() > 0)
		{
			const FColorVertexBuffer& CB = RD->LODResources[0].VertexBuffers.ColorVertexBuffer;
			if (CB.GetNumVertices() > 0)
			{
				const FColor B = CB.VertexColor(0);
				Out += FString::Printf(TEXT(" buffer=(%d %d %d %d)"), B.R, B.G, B.B, B.A);
			}
			else Out += TEXT(" buffer=none");
		}
	}
	return Out;
}

FString UMCEditorLibrary::BuildAllContent()
{
	FString Log;
	int32 Made = 0;
	for (int32 L = 0; L < 7; ++L)
		if (BuildVoxelMaterial((EMCOpus55Layer)L, false)) ++Made;
	if (BuildEntityMaterial()) ++Made;
	if (BuildGlowMaterial()) ++Made;
	if (BuildBeamMaterial()) ++Made;
	if (BuildSkyMaterial()) ++Made;
	if (BuildCloudMaterial()) ++Made;
	Log += FString::Printf(TEXT("materials built: %d\n"), Made);
	Log += FString::Printf(TEXT("default map: %s\n"), BuildDefaultMap() ? TEXT("saved") : TEXT("FAILED"));
	Log += FString::Printf(TEXT("hero meshes imported: %d\n"), ImportHeroMeshes());
	Log += VerifyContent();
	UE_LOG(LogOpus55Editor, Display, TEXT("Opus 5.5 content build:\n%s"), *Log);
	return Log;
}
