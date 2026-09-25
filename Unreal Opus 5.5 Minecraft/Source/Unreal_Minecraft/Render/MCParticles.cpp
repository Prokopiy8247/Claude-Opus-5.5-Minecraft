// CPU particle system: billboard quads built into one procedural mesh per frame with the terrain texture array.
#include "Render/MCParticles.h"
#include "Render/MCChunkMeshComponent.h"
#include "Render/MCTextureSynth.h"
#include "World/MCWorld.h"
#include "Blocks/MCBlocks.h"
#include "Blocks/MCTextures.h"
#include "Gen/MCBiomes.h"
#include "Render/MCMesher.h"
#include "Game/MCGame.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MCParticles)

using namespace MCTexSynth;

namespace
{
	struct FEffectDef
	{
		int32 Sprite = PS_Smoke;     // -1 = block textures
		FColor Color = FColor::White;
		int32 Count = 1;
		float Spread = 0.2f;
		float Speed = 0.05f;
		float UpBias = 0.f;
		float Gravity = -2.5f;
		float Life = 1.0f;
		float LifeVar = 0.4f;
		float Size = 0.10f;
		float SizeVar = 0.4f;
		float Drag = 0.92f;
		bool bEmissive = false;
		bool bCollide = true;
		bool bNoShade = false;       // ignore biome/block lighting (glowing sprites)
	};

	const float G = -24.f; // blocks/s^2 (Minecraft gravity scaled to ticks->seconds)

	FEffectDef Make(int32 Sprite, uint32 Col, int32 Count, float Spread, float Speed, float Up, float Grav, float Life, float LifeVar, float Size, uint8 A = 255)
	{
		FEffectDef D;
		D.Sprite = Sprite;
		D.Color = FColor((Col >> 16) & 255, (Col >> 8) & 255, Col & 255, A);
		D.Count = Count; D.Spread = Spread; D.Speed = Speed; D.UpBias = Up; D.Gravity = Grav;
		D.Life = Life; D.LifeVar = LifeVar; D.Size = Size;
		return D;
	}
}

UMCParticles::UMCParticles(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	Rand.SetSeed(0xA57A1u);
}

void UMCParticles::EnsureMesh()
{
	if (Mesh) return;
	AActor* Owner = GetOwner();
	Mesh = NewObject<UMCChunkMeshComponent>(Owner ? (UObject*)Owner : (UObject*)this, TEXT("ParticleMesh"));
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCastShadow(false);
	Mesh->bAffectDynamicIndirectLighting = false;
	Mesh->bAffectDistanceFieldLighting = false;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SharedMaterials = MCRender::GVoxelMaterials;
	if (Owner && Owner->GetRootComponent()) Mesh->SetupAttachment(Owner->GetRootComponent());
	Mesh->SetUsingAbsoluteLocation(true);
	Mesh->SetUsingAbsoluteRotation(true);
	Mesh->RegisterComponent();
}

void UMCParticles::Add(const FMCParticle& P)
{
	if (Particles.Num() >= MaxParticles)
	{
		// replace the oldest particle so important effects are never starved
		Particles.RemoveAtSwap(0, 1, EAllowShrinking::No);
	}
	Particles.Add(P);
}

// ---------------------------------------------------------------------------------------------------------------------
// Named effects

void UMCParticles::Spawn(FName Type, const FVector& Pos, int32 Count, float Spread, const FVector& Vel, FColor Color)
{
	if (!World) return;
	const FString N = Type.ToString();
	FEffectDef D;
	int32 Injected = 0;
	bool bKnown = true;

	// ---- one-shot events with fixed parameters
	if (N.Contains(TEXT("smoke"))) { D = Make(PS_Smoke, 0x2A2A2A, N.Contains(TEXT("large")) ? 12 : 6, 0.3f, 0.02f, 0.8f, -0.6f, 0.9f, 0.5f, N.Contains(TEXT("large")) ? 0.32f : 0.16f, 190); }
	else if (N.Contains(TEXT("flame")) || N == TEXT("soul_fire_flame")) { D = Make(N.Contains(TEXT("soul")) ? PS_SoulFlame : PS_Flame, 0xFFB040, 3, 0.2f, 0.01f, 1.4f, -0.35f, 0.7f, 0.3f, 0.14f); D.bEmissive = true; D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("lava_pop") || N == TEXT("lava")) { D = Make(PS_Flame, 0xFF7A20, 4, 0.4f, 0.05f, 2.4f, -G * 0.5f, 0.6f, 0.3f, 0.12f); D.bEmissive = true; D.bNoShade = true; }
	else if (N == TEXT("portal")) { D = Make(PS_Portal, 0xA050FF, 3, 0.6f, 0.02f, 0.3f, 0.f, 1.6f, 0.6f, 0.14f); D.bEmissive = true; D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("end_rod")) { D = Make(PS_Spark, 0xF4EEE8, 3, 0.1f, 0.005f, 0.f, 0.f, 2.0f, 0.6f, 0.10f); D.bEmissive = true; D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("heart")) { D = Make(PS_Heart, 0xFFFFFF, 2, 0.4f, 0.02f, 0.9f, 0.f, 1.0f, 0.3f, 0.16f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("angry_villager")) { D = Make(PS_Angry, 0xFFFFFF, 3, 0.4f, 0.01f, 0.6f, 0.f, 0.9f, 0.3f, 0.14f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("happy_villager")) { D = Make(PS_Happy, 0xFFFFFF, 3, 0.4f, 0.01f, 0.6f, 0.f, 0.9f, 0.3f, 0.14f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("crit") || N == TEXT("enchanted_hit")) { D = Make(PS_Crit, N.StartsWith(TEXT("enchanted")) ? 0x50E0C0 : 0xFFE080, 6, 0.35f, 0.04f, 0.4f, -1.5f, 0.5f, 0.2f, 0.14f); D.bEmissive = true; D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("effect") || N == TEXT("splash") || N == TEXT("splash_potion") || N == TEXT("spell")) { D = Make(PS_Rune, N.StartsWith(TEXT("splash")) ? 0xE060A0 : 0x8040C0, 24, 0.6f, 0.03f, 0.2f, -3.f, 0.9f, 0.4f, 0.13f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("explosion") || N == TEXT("explosion_emitter")) { D = Make(PS_Explosion, 0xFFE0B0, 30, 0.8f, 0.10f, 0.25f, -1.f, 0.8f, 0.4f, 0.7f); D.bEmissive = true; D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("poof")) { D = Make(PS_Poof, 0xE8E8E8, 8, 0.4f, 0.03f, 0.3f, -0.5f, 0.7f, 0.3f, 0.2f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("bubble") || N == TEXT("bubble_pop")) { D = Make(PS_Bubble, 0xFFFFFF, 3, 0.3f, 0.02f, 1.2f, 0.8f, 1.0f, 0.4f, 0.08f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("drip_water") || N == TEXT("water_drip")) { D = Make(PS_Drip, 0x6070E8, 1, 0.1f, 0.f, -0.6f, -G, 0.7f, 0.2f, 0.06f); D.bNoShade = true; D.bCollide = true; }
	else if (N.Contains(TEXT("drip")) && N.Contains(TEXT("lava"))) { D = Make(PS_Drip, 0xE8520A, 1, 0.1f, 0.f, -0.6f, -G, 0.7f, 0.2f, 0.06f); D.bEmissive = true; D.bNoShade = true; }
	else if (N == TEXT("snowflake") || N == TEXT("snow_fall")) { D = Make(PS_Snow, 0xFFFFFF, 2, 0.8f, 0.02f, -0.9f, -0.4f, 2.5f, 1.f, 0.07f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("rain_fall") || N == TEXT("rain_splash")) { D = Make(PS_Rain, 0xA8C8FF, 3, 0.8f, 0.02f, -3.f, -G, 0.6f, 0.2f, 0.08f); D.bNoShade = true; D.bCollide = false; D.Count = N.Contains(TEXT("splash")) ? 6 : 3; }
	else if (N == TEXT("note") || N == TEXT("note_ambient")) { D = Make(PS_Note, 0xFFFFFF, 1, 0.5f, 0.01f, 0.7f, 0.f, 1.2f, 0.3f, 0.16f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("totem") || N == TEXT("totem_of_undying")) { D = Make(PS_Happy, 0x60FF80, 40, 0.6f, 0.12f, 0.4f, -1.f, 1.6f, 0.4f, 0.22f); D.bEmissive = true; D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("sweep_attack")) { D = Make(PS_Sweep, 0xFFFFFF, 1, 0.f, 0.f, 0.f, 0.f, 0.4f, 0.f, 1.2f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("sonic_boom")) { D = Make(PS_Poof, 0x60E0FF, 24, 0.8f, 0.2f, 0.f, 0.f, 0.6f, 0.2f, 0.5f); D.bEmissive = true; D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("gust") || N == TEXT("gust_emitter") || N == TEXT("wind_charge")) { D = Make(PS_Poof, 0xE8F0FF, 10, 0.8f, 0.14f, 0.f, 0.f, 0.5f, 0.2f, 0.4f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("firework_spark")) { D = Make(PS_Spark, 0xFFE080, 2, 0.15f, 0.03f, 0.f, -1.5f, 0.5f, 0.2f, 0.08f); D.bEmissive = true; D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("firework_burst")) { D = Make(PS_Spark, 0xFFFFFF, 60, 0.3f, 0.35f, 0.1f, -2.f, 1.4f, 0.4f, 0.12f); D.bEmissive = true; D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("dragon_breath")) { D = Make(PS_Poof, 0xB060E0, 12, 0.5f, 0.03f, 0.2f, -0.6f, 1.4f, 0.5f, 0.24f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("sculk_soul")) { D = Make(PS_Spark, 0x2AE8F0, 4, 0.4f, 0.02f, 0.5f, 0.f, 1.4f, 0.5f, 0.12f); D.bEmissive = true; D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("soul") || N == TEXT("soul_escape")) { D = Make(PS_SoulFlame, 0x40E0F0, 3, 0.5f, 0.02f, 0.6f, 0.f, 1.6f, 0.6f, 0.14f); D.bEmissive = true; D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("spore_blossom_air") || N == TEXT("spore")) { D = Make(PS_Leaf, 0xE870B8, 4, 1.2f, 0.01f, -0.3f, -0.3f, 3.0f, 1.f, 0.1f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("falling_leaf") || N == TEXT("cherry_leaves") || N == TEXT("leaf")) { D = Make(PS_Leaf, N.StartsWith(TEXT("cherry")) ? 0xF0B0C8 : 0x7AA83A, 3, 0.8f, 0.01f, -0.25f, -0.35f, 2.6f, 1.f, 0.11f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("sulfur_spark")) { D = Make(PS_Spark, 0xF0DA5A, 6, 0.4f, 0.06f, 0.6f, -2.f, 0.8f, 0.3f, 0.1f); D.bEmissive = true; D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("firefly")) { D = Make(PS_Spark, 0xE8FF7A, 3, 1.4f, 0.01f, 0.1f, 0.f, 4.0f, 1.f, 0.08f); D.bEmissive = true; D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("mycelium")) { D = Make(PS_Poof, 0x8E7C8A, 3, 0.6f, 0.005f, 0.15f, 0.f, 2.0f, 0.6f, 0.14f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("wax_on") || N == TEXT("wax_off")) { D = Make(PS_Spark, N.StartsWith(TEXT("wax_on")) ? 0xFFD060 : 0xE8E8E8, 8, 0.4f, 0.05f, 0.4f, -1.5f, 0.6f, 0.2f, 0.1f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("composter")) { D = Make(PS_Poof, 0x6A5A2E, 4, 0.4f, 0.02f, 0.3f, -1.f, 0.8f, 0.3f, 0.12f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("spit") || N == TEXT("llama_spit")) { D = Make(PS_Bubble, 0xD8D8C0, 4, 0.2f, 0.08f, 0.2f, -G, 0.5f, 0.2f, 0.08f); D.bCollide = false; }
	else if (N == TEXT("dust") || N == TEXT("dust_plume")) { D = Make(PS_Dust, 0x9A8A70, 6, 0.4f, 0.05f, 0.15f, -1.f, 0.7f, 0.3f, 0.18f); D.bNoShade = true; }
	else if (N == TEXT("damage_indicator")) { D = Make(PS_Crit, 0xFF4040, 2, 0.3f, 0.02f, 0.5f, 0.f, 0.8f, 0.2f, 0.14f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("evoker_fangs")) { D = Make(PS_Crit, 0xC8C8D8, 6, 0.3f, 0.04f, 0.9f, -3.f, 0.5f, 0.2f, 0.14f); D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("sculk_charge") || N == TEXT("sculk_charge_pop")) { D = Make(PS_SoulFlame, 0x2AE8F0, 3, 0.3f, 0.02f, 0.4f, 0.f, 0.8f, 0.3f, 0.14f); D.bEmissive = true; D.bNoShade = true; D.bCollide = false; }
	else if (N == TEXT("block_dust") || N == TEXT("dust_block")) { D = Make(PS_Dust, Color.DWColor() & 0xFFFFFF, 8, 0.35f, 0.04f, 0.15f, -G * 0.4f, 0.6f, 0.2f, 0.16f); D.bNoShade = false; }
	else if (N == TEXT("item_break") || N == TEXT("item_crumbs")) { D = Make(-1, 0xFFFFFF, 8, 0.25f, 0.06f, 0.25f, -G, 0.7f, 0.3f, 0.09f); }
	else if (N == TEXT("block") || N == TEXT("block_crumbs")) { D = Make(-1, 0xFFFFFF, 10, 0.3f, 0.05f, 0.2f, -G, 0.7f, 0.3f, 0.12f); }
	else bKnown = false;

	// ---- explicit overrides from the caller
	if (Count > 0) D.Count = Count;
	if (Spread > 0.f) D.Spread = Spread;
	if (Color != FColor::White) D.Color = Color;
	if (!bKnown)
	{
		// unknown / generic: use the layered data-driven defaults from the table below
		D = Make(PS_Smoke, 0xCCCCCC, 4, 0.3f, 0.03f, 0.3f, -1.f, 1.0f, 0.4f, 0.14f, 200);
		if (N.Contains(TEXT("flame")) || N.Contains(TEXT("fire"))) { D.Sprite = PS_Flame; D.bEmissive = true; D.bNoShade = true; D.Color = FColor(255, 176, 64); }
		else if (N.Contains(TEXT("soul"))) { D.Sprite = PS_SoulFlame; D.bEmissive = true; D.bNoShade = true; D.Color = FColor(64, 224, 240); }
		else if (N.Contains(TEXT("portal")) || N.Contains(TEXT("end"))) { D.Sprite = PS_Portal; D.bEmissive = true; D.bNoShade = true; D.Color = FColor(160, 80, 255); }
		else if (N.Contains(TEXT("bubble")) || N.Contains(TEXT("splash"))) { D.Sprite = PS_Bubble; D.Color = FColor(200, 230, 255); }
		else if (N.Contains(TEXT("heart"))) D.Sprite = PS_Heart;
		else if (N.Contains(TEXT("crit")) || N.Contains(TEXT("spark"))) { D.Sprite = PS_Spark; D.bNoShade = true; }
		else if (N.Contains(TEXT("leaf"))) { D.Sprite = PS_Leaf; D.bNoShade = true; }
		else if (N.Contains(TEXT("snow"))) { D.Sprite = PS_Snow; D.bNoShade = true; }
		else if (N.Contains(TEXT("rain"))) { D.Sprite = PS_Rain; D.bNoShade = true; }
		else if (N.Contains(TEXT("note"))) { D.Sprite = PS_Note; D.bNoShade = true; }
		else if (N.Contains(TEXT("smoke")) || N.Contains(TEXT("poof"))) D.Sprite = PS_Smoke;
		Injected = 1;
	}
	(void)Injected;

	FMCBiomeDef Biome = FMCBiomes::Get(World->GetBiome(FMCBlockPos::FromWorld(Pos)));
	for (int32 i = 0; i < D.Count; ++i)
	{
		FMCParticle P;
		P.Pos = Pos + FVector(Rand.FRange(-D.Spread, D.Spread), Rand.FRange(-D.Spread, D.Spread), (D.Sprite == PS_Leaf || D.Sprite == PS_Snow || D.Sprite == PS_Rain) ? Rand.FRange(-0.2f, 0.2f) : Rand.FRange(-D.Spread * 0.4f, D.Spread * 0.4f));
		// effect speeds are authored in blocks per tick (Minecraft convention); simulation runs in blocks per second
		P.Vel = (FVector(Rand.FRange(-1.f, 1.f) * D.Speed, Rand.FRange(-1.f, 1.f) * D.Speed, D.UpBias * D.Speed + Rand.FRange(0.f, D.Speed * 0.5f)) + Vel) * 20.f;
		P.Age = 0.f;
		P.Life = FMath::Max(0.05f, D.Life * Rand.FRange(1.f - D.LifeVar, 1.f + D.LifeVar));
		P.Size = D.Size * Rand.FRange(1.f - D.SizeVar, 1.f + D.SizeVar);
		P.Gravity = D.Gravity;
		P.Drag = D.Drag;
		P.bEmissive = D.bEmissive;
		P.bCollide = D.bCollide && D.Sprite == -1;
		P.bShrink = D.Sprite != -1;
		P.Spin = Rand.FRange(0.f, 360.f);
		if (D.Sprite < 0)
		{
			P.Kind = 1;
			P.TexLayer = FMCBlocks::Info(0).Tex[0]; // replaced by SpawnBlockBreak / SpawnBlockHit
			P.Size = FMath::Max(P.Size, 0.09f);
		}
		else
		{
			P.Kind = 0;
			P.TexLayer = (int16)ParticleLayer(D.Sprite);
		}
		FColor C = D.Color;
		if (FMath::Abs(C.R - 255) + FMath::Abs(C.G - 255) + FMath::Abs(C.B - 255) < 6 && D.Sprite != PS_Explosion)
		{
			// white particles pick up the biome colour like vanilla grass/leaf particles
			C = Biome.Foliage;
			if (D.Sprite == PS_Rain) C = Biome.Water;
		}
		const float CV = Rand.FRange(0.85f, 1.15f);
		P.Color = FLinearColor(FMath::Min(1.f, C.R / 255.f * CV), FMath::Min(1.f, C.G / 255.f * CV), FMath::Min(1.f, C.B / 255.f * CV), D.Color.A / 255.f);
		Add(P);
		(void)Injected;
	}
}

void UMCParticles::SpawnBlockBreak(const FMCBlockPos& P, uint16 State)
{
	if (!World) return;
	const FMCStateInfo& SI = FMCBlocks::Info(State);
	if (SI.Block == 0) return;
	const FMCBlock& B = FMCBlocks::Get(SI.Block);
	const int32 Pieces = 22;
	for (int32 i = 0; i < Pieces; ++i)
	{
		FMCParticle Pt;
		const int32 Face = Rand.NextInt(6);
		const int16 Tex = SI.Tex[Face] >= 0 ? SI.Tex[Face] : B.Tex[Face];
		Pt.Kind = 1;
		Pt.TexLayer = FMath::Max<int16>(0, Tex);
		Pt.Pos = FVector(P.X + Rand.FRange(0.1f, 0.9f), P.Y + Rand.FRange(0.1f, 0.9f), P.Z + Rand.FRange(0.1f, 0.9f));
		Pt.Vel = FVector(Rand.FRange(-0.09f, 0.09f), Rand.FRange(-0.09f, 0.09f), Rand.FRange(0.02f, 0.12f)) * 20.f;
		Pt.Life = Rand.FRange(0.5f, 1.1f);
		Pt.Size = Rand.FRange(0.08f, 0.16f);
		Pt.Gravity = G;
		Pt.Drag = 0.86f;
		Pt.bCollide = true;
		Pt.Spin = Rand.FRange(0.f, 360.f);
		const FColor T = B.Tint == EMCTint::None ? FColor::White : FMCBiomes::Get(World->GetBiome(P)).Grass;
		Pt.Color = FLinearColor(T.R / 255.f, T.G / 255.f, T.B / 255.f, 1.f);
		const FVector2f UV(Rand.NextInt(4) * 0.25f, Rand.NextInt(4) * 0.25f);
		Pt.UVMin = UV;
		Pt.UVMax = UV + FVector2f(0.25f, 0.25f);
		Add(Pt);
	}
	EnsureMesh();
}

void UMCParticles::SpawnBlockHit(const FMCBlockPos& P, uint16 State, EMCFace Face)
{
	if (!World) return;
	const FMCStateInfo& SI = FMCBlocks::Info(State);
	if (SI.Block == 0) return;
	const FMCBlock& B = FMCBlocks::Get(SI.Block);
	const FIntVector& D = MC::FaceDir[(int32)Face];
	for (int32 i = 0; i < 5; ++i)
	{
		FMCParticle Pt;
		Pt.Kind = 1;
		Pt.TexLayer = FMath::Max<int16>(0, SI.Tex[(int32)Face] >= 0 ? SI.Tex[(int32)Face] : B.Tex[(int32)Face]);
		const FVector Base(P.X + 0.5f + D.X * 0.52f, P.Y + 0.5f + D.Y * 0.52f, P.Z + 0.5f + D.Z * 0.52f);
		Pt.Pos = Base + FVector(Rand.FRange(-0.3f, 0.3f), Rand.FRange(-0.3f, 0.3f), Rand.FRange(-0.3f, 0.3f));
		Pt.Vel = (FVector(D.X, D.Y, D.Z) * 0.02f + FVector(Rand.FRange(-0.03f, 0.03f), Rand.FRange(-0.03f, 0.03f), Rand.FRange(0.01f, 0.04f))) * 20.f;
		Pt.Life = Rand.FRange(0.3f, 0.6f);
		Pt.Size = Rand.FRange(0.06f, 0.1f);
		Pt.Gravity = G;
		Pt.bCollide = false;
		const FColor T = B.Tint == EMCTint::None ? FColor::White : FMCBiomes::Get(World->GetBiome(P)).Grass;
		Pt.Color = FLinearColor(T.R / 255.f, T.G / 255.f, T.B / 255.f, 1.f);
		Add(Pt);
	}
	EnsureMesh();
}

// ---------------------------------------------------------------------------------------------------------------------
// Simulation & rendering

void UMCParticles::UpdateParticles(float DeltaSeconds, const FVector& CameraPosUU, const FRotator& CameraRot)
{
	if (!World) return;
	const double Time = FPlatformTime::Seconds();
	(void)Time;
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	const FVector CamB = CameraPosUU * MC::InvBlockSize;
	const FVector Fwd = CameraRot.Vector();
	const FVector RightV = FRotator(0, CameraRot.Yaw + 90.f, 0).Vector();
	const FVector UpV = FVector::CrossProduct(RightV, Fwd).GetSafeNormal();

	int32 Write = 0;
	for (int32 i = 0; i < Particles.Num(); ++i)
	{
		FMCParticle& P = Particles[i];
		P.Age += Dt;
		if (P.Age >= P.Life) continue;
		if ((P.Pos - CamB).SizeSquared() > 64.0 * 64.0) continue;
		P.Vel.Z += P.Gravity * Dt;
		const float DragF = FMath::Pow(P.Drag, Dt * 20.f);
		P.Vel *= DragF;
		const FVector Delta = P.Vel * Dt;
		const FVector Next = P.Pos + Delta;
		if (P.bCollide)
		{
			const FMCBox Box(Next - FVector(P.Size * 0.5f), Next + FVector(P.Size * 0.5f));
			if (World->IsRegionFree(Box)) P.Pos = Next;
			else
			{
				// bounce along the axis that is free
				const FMCBox XBox(FVector(Next.X - P.Size * 0.5f, P.Pos.Y - P.Size * 0.5f, P.Pos.Z - P.Size * 0.5f), FVector(Next.X + P.Size * 0.5f, P.Pos.Y + P.Size * 0.5f, P.Pos.Z + P.Size * 0.5f));
				if (World->IsRegionFree(XBox)) { P.Pos.X = Next.X; P.Vel.Y *= -0.2f; }
				else
				{
					const FMCBox YBox(FVector(P.Pos.X - P.Size * 0.5f, Next.Y - P.Size * 0.5f, P.Pos.Z - P.Size * 0.5f), FVector(P.Pos.X + P.Size * 0.5f, Next.Y + P.Size * 0.5f, P.Pos.Z + P.Size * 0.5f));
					if (World->IsRegionFree(YBox)) { P.Pos.Y = Next.Y; P.Vel.X *= -0.2f; }
					else
					{
						P.Vel.Z = 0.f;
						P.Vel.X *= 0.5f;
						P.Vel.Y *= 0.5f;
						P.bOnGround = true;
					}
				}
			}
		}
		else P.Pos = Next;
		Particles[Write++] = P;
	}
	Particles.SetNum(Write, EAllowShrinking::No);
	if (Particles.Num() == 0)
	{
		if (Mesh && Mesh->HasGeometry()) Mesh->ClearMesh();
		return;
	}
	EnsureMesh();
	if (!Mesh) return;

	// ---- build one billboard quad per particle
	const float Alpha = 1.f;
	TUniquePtr<FMCChunkMeshData> Data = MakeUnique<FMCChunkMeshData>();
	FMCMeshLayerData& Cut = Data->Layers[(int32)EMCLayer::Cutout];
	FMCMeshLayerData& Trans = Data->Layers[(int32)EMCLayer::Translucent];
	// vertices are relative to a snapped origin near the camera (float precision far from 0,0)
	const FVector Origin(FMath::GridSnap(CameraPosUU.X, 1600.0), FMath::GridSnap(CameraPosUU.Y, 1600.0), FMath::GridSnap(CameraPosUU.Z, 1600.0));
	const int32 Darken = World->Game ? World->Game->GetSkyDarken() : 0;

	for (const FMCParticle& P : Particles)
	{
		const float T = P.Age / P.Life;
		float Size = P.Size;
		float A = Alpha;
		if (P.bShrink) Size *= FMath::Lerp(1.25f, 0.35f, T);
		if (P.bFade) A *= FMath::Clamp(1.f - (T - 0.6f) / 0.4f, 0.f, 1.f);
		if (A <= 0.01f) continue;

		// camera-facing quad with a roll for block chips
		FVector RX = RightV, UY = UpV;
		if (P.Spin != 0.f)
		{
			const float Rad = FMath::DegreesToRadians(P.Spin + (P.Kind == 1 ? P.Age * 90.f : P.Age * 20.f));
			RX = FVector(FMath::Cos(Rad) * RightV.X + FMath::Sin(Rad) * UpV.X, FMath::Cos(Rad) * RightV.Y + FMath::Sin(Rad) * UpV.Y, FMath::Cos(Rad) * RightV.Z + FMath::Sin(Rad) * UpV.Z);
			UY = FVector(-FMath::Sin(Rad) * RightV.X + FMath::Cos(Rad) * UpV.X, -FMath::Sin(Rad) * RightV.Y + FMath::Cos(Rad) * UpV.Y, -FMath::Sin(Rad) * RightV.Z + FMath::Cos(Rad) * UpV.Z);
		}
		const FVector C = P.Pos;
		const FVector Half = (RX * (Size * 0.5f) + UY * (Size * 0.5f)) * MC::BlockSize;
		const FVector P0 = C * MC::BlockSize - Origin - RX * (Size * 0.5f) * MC::BlockSize - UY * (Size * 0.5f) * MC::BlockSize;
		const FVector P1 = P0 + RX * Size * MC::BlockSize;
		const FVector P2 = P1 + UY * Size * MC::BlockSize;
		const FVector P3 = P0 + UY * Size * MC::BlockSize;
		(void)Half;

		const FMCBlockPos BP = FMCBlockPos(MC::FloorToInt(P.Pos.X), MC::FloorToInt(P.Pos.Y), MC::FloorToInt(P.Pos.Z));
		// Minecraft-style brightness curve from the combined light level (sky light darkened at night)
		const float Lv = FMath::Clamp(World->GetLight(BP, Darken) / 15.f, 0.f, 1.f);
		const float Shade = P.bEmissive ? 1.f : FMath::Max(0.08f, Lv / (4.f - 3.f * Lv));
		FMCMeshLayerData& L = P.Kind == 1 ? Cut : Trans;
		const uint32 Base = (uint32)L.Vertices.Num();
		const FVector Corners[4] = { P0, P1, P2, P3 };
		const FVector2f UV[4] = { FVector2f(P.UVMin.X, P.UVMax.Y), FVector2f(P.UVMax.X, P.UVMax.Y), FVector2f(P.UVMax.X, P.UVMin.Y), FVector2f(P.UVMin.X, P.UVMin.Y) };
		for (int32 k = 0; k < 4; ++k)
		{
			FMCMeshVertex V;
			V.Pos = FVector3f(Corners[k]);
			V.Normal = FVector3f(-Fwd);
			V.Tangent = FVector3f(RX);
			V.TangentSign = 1.f;
			V.UV0 = UV[k];
			V.UV1 = FVector2f((float)P.TexLayer, (float)MCRender::VF_Particle);
			V.UV2 = FVector2f(1.f, 1.f);
			V.Color = FColor((uint8)(FMath::Clamp(P.Color.R * Shade, 0.f, 1.f) * 255.f), (uint8)(FMath::Clamp(P.Color.G * Shade, 0.f, 1.f) * 255.f),
				(uint8)(FMath::Clamp(P.Color.B * Shade, 0.f, 1.f) * 255.f), (uint8)(FMath::Clamp(P.Color.A * A, 0.f, 1.f) * 255.f));
			L.Vertices.Add(V);
		}
		L.Indices.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
	}
	Data->Bounds = FBox(ForceInit);
	for (const FMCParticle& P : Particles) Data->Bounds += P.Pos * MC::BlockSize - Origin;
	Data->Bounds = Data->Bounds.ExpandBy(200.f);
	if (MCRender::GVoxelMaterials) Mesh->SharedMaterials = MCRender::GVoxelMaterials;
	Mesh->SetWorldLocation(Origin);
	Mesh->SetMeshData(MoveTemp(Data));
}
