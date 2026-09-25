#include "Blocks/MCBlocks.h"
#include "Blocks/MCTextures.h"
#include "Blocks/MCBlockBehavior.h"

TArray<FMCBlock> FMCBlocks::Blocks;
TArray<FMCStateInfo> FMCBlocks::StateInfos;
TMap<FName, FMCBlockId> FMCBlocks::NameToId;
TArray<FMCBlockModel> FMCBlocks::StaticModels;
bool FMCBlocks::bInitialized = false;
FMCBlocks::FCommon FMCBlocks::C;

// Implemented in MCBlockRegistry*.cpp / MCBlockModels.cpp
void MCRegisterAllBlocks(TArray<FMCBlock>& Blocks);
void MCBuildBlockModel(const FMCBlock& B, uint8 Meta, const FMCBlockGetter* Getter, const FMCBlockPos& Pos, FMCBlockModel& Out);
bool MCIsDynamicModel(const FMCBlock& B);

static FMCBlockBehavior GDefaultBehavior;

void FMCModelBox::RotateY(int32 Quarter)
{
	Quarter = ((Quarter % 4) + 4) % 4;
	if (Quarter == 0) return;
	const FMCBox R = ToBox().RotateY(Quarter);
	Min = FVector3f(R.Min);
	Max = FVector3f(R.Max);
	// permute side faces: North -> East -> South -> West
	static const int32 Cycle[4] = { (int32)EMCFace::North, (int32)EMCFace::East, (int32)EMCFace::South, (int32)EMCFace::West };
	for (int32 Q = 0; Q < Quarter; ++Q)
	{
		int16 T[6]; FVector4f U[6]; uint8 Rt[6];
		for (int32 i = 0; i < 6; ++i) { T[i] = Tex[i]; U[i] = UV[i]; Rt[i] = UVRot[i]; }
		uint8 NewCull = CullFaces & 0x3, NewTint = TintFaces & 0x3;
		for (int32 i = 0; i < 4; ++i)
		{
			const int32 From = Cycle[i], To = Cycle[(i + 1) % 4];
			Tex[To] = T[From]; UV[To] = U[From]; UVRot[To] = Rt[From];
			if (CullFaces & (1 << From)) NewCull |= 1 << To;
			if (TintFaces & (1 << From)) NewTint |= 1 << To;
		}
		CullFaces = NewCull;
		TintFaces = NewTint;
		UVRot[(int32)EMCFace::Up] = (UVRot[(int32)EMCFace::Up] + 1) & 3;
		UVRot[(int32)EMCFace::Down] = (UVRot[(int32)EMCFace::Down] + 3) & 3;
	}
}

void FMCModelQuad::RotateY(int32 Quarter)
{
	Quarter = ((Quarter % 4) + 4) % 4;
	for (int32 Q = 0; Q < Quarter; ++Q)
	{
		for (FVector3f& V : P)
		{
			const float X = V.X, Y = V.Y;
			V.X = 1.f - Y; V.Y = X;
		}
	}
}

bool FMCBlocks::IsInitialized() { return bInitialized; }

void FMCBlocks::Init()
{
	if (bInitialized) return;
	FMCTextures::Init();
	Blocks.Reset();
	NameToId.Reset();
	MCRegisterAllBlocks(Blocks);
	Finalize();
	bInitialized = true;
	UE_LOG(LogOpus55, Log, TEXT("Registered %d blocks / %d states / %d static models"), Blocks.Num(), StateInfos.Num(), StaticModels.Num());
}

const FMCBlock& FMCBlocks::Get(FMCBlockId Id)
{
	return Blocks.IsValidIndex(Id) ? Blocks[Id] : Blocks[0];
}

const FMCBlock* FMCBlocks::Find(FName Name)
{
	if (const FMCBlockId* Id = NameToId.Find(Name)) return &Blocks[*Id];
	return nullptr;
}

FMCBlockId FMCBlocks::FindId(FName Name)
{
	if (const FMCBlockId* Id = NameToId.Find(Name)) return *Id;
	return 0;
}

FMCState FMCBlocks::FindState(FName Name, uint16 Meta)
{
	if (const FMCBlock* B = Find(Name)) return B->State(Meta);
	return 0;
}

int32 FMCBlocks::NumBlocks() { return Blocks.Num(); }

static void ResolveCubeTextures(const FMCBlock& B, uint8 Meta, int16 OutTex[6], uint8 OutRot[6])
{
	for (int32 i = 0; i < 6; ++i) { OutTex[i] = B.Tex[i]; OutRot[i] = 0; }
	const int32 D = (int32)EMCFace::Down, U = (int32)EMCFace::Up, N = (int32)EMCFace::North, S = (int32)EMCFace::South, W = (int32)EMCFace::West, E = (int32)EMCFace::East;
	switch (B.Orient)
	{
	case EMCCubeOrient::Axis:
	{
		const int16 Top = B.Tex[U], Bottom = B.Tex[D], Side = B.Tex[N];
		const uint8 Axis = MCMeta::Axis(Meta);
		if (Axis == 1) // X axis: west/east show the end grain
		{
			OutTex[W] = Top; OutTex[E] = Top;
			OutTex[U] = Side; OutTex[D] = Side; OutTex[N] = Side; OutTex[S] = Side;
			OutRot[U] = 1; OutRot[D] = 1; OutRot[N] = 1; OutRot[S] = 1;
		}
		else if (Axis == 2) // Y axis: north/south show the end grain
		{
			OutTex[N] = Top; OutTex[S] = Top;
			OutTex[U] = Side; OutTex[D] = Side; OutTex[W] = Side; OutTex[E] = Side;
			OutRot[W] = 1; OutRot[E] = 1;
		}
		else
		{
			OutTex[U] = Top; OutTex[D] = Bottom >= 0 ? Bottom : Top;
			OutTex[N] = OutTex[S] = OutTex[W] = OutTex[E] = Side;
		}
		break;
	}
	case EMCCubeOrient::Facing4:
	case EMCCubeOrient::Facing4Lit:
	{
		const EMCFace F = MCMeta::Facing4(Meta);
		const bool bLit = B.Orient == EMCCubeOrient::Facing4Lit && MCMeta::Bit(Meta, 2);
		const int16 Front = (bLit && B.TexAlt[0] >= 0) ? B.TexAlt[0] : B.Tex[N];
		const int16 Back = B.Tex[S], Side = B.Tex[E];
		OutTex[N] = OutTex[S] = OutTex[W] = OutTex[E] = Side;
		OutTex[(int32)F] = Front;
		OutTex[(int32)MC::Opposite(F)] = Back;
		OutTex[U] = B.Tex[U]; OutTex[D] = B.Tex[D];
		OutRot[U] = (uint8)MCMeta::QuarterFromNorth(F);
		break;
	}
	case EMCCubeOrient::Facing6:
	case EMCCubeOrient::Facing6Lit:
	{
		const EMCFace F = MCMeta::Facing6(Meta);
		const bool bActive = B.Orient == EMCCubeOrient::Facing6Lit && MCMeta::Bit(Meta, 3);
		const int16 Front = B.Tex[N];
		const int16 Back = (bActive && B.TexAlt[0] >= 0) ? B.TexAlt[0] : B.Tex[S];
		const int16 Side = B.Tex[E];
		const int16 TopSide = B.Tex[U];
		if (F == EMCFace::Up || F == EMCFace::Down)
		{
			const int16 Vertical = B.TexAlt[1] >= 0 ? B.TexAlt[1] : Front;
			OutTex[(int32)F] = Vertical;
			OutTex[(int32)MC::Opposite(F)] = Back;
			OutTex[N] = OutTex[S] = OutTex[W] = OutTex[E] = TopSide;
			if (F == EMCFace::Down) { OutRot[N] = OutRot[S] = OutRot[W] = OutRot[E] = 2; }
		}
		else
		{
			OutTex[N] = OutTex[S] = OutTex[W] = OutTex[E] = Side;
			OutTex[(int32)F] = Front;
			OutTex[(int32)MC::Opposite(F)] = Back;
			OutTex[U] = TopSide; OutTex[D] = TopSide;
			OutRot[U] = (uint8)MCMeta::QuarterFromNorth(F);
			OutRot[D] = (uint8)((4 - MCMeta::QuarterFromNorth(F)) & 3);
		}
		break;
	}
	case EMCCubeOrient::Snowy:
		if (MCMeta::Bit(Meta, 0) && B.TexAlt[0] >= 0)
		{
			OutTex[N] = OutTex[S] = OutTex[W] = OutTex[E] = B.TexAlt[0];
		}
		break;
	case EMCCubeOrient::Upper:
		if (MCMeta::Bit(Meta, 0) && B.TexAlt[0] >= 0)
		{
			for (int32 i = 0; i < 6; ++i) OutTex[i] = B.TexAlt[0];
		}
		break;
	case EMCCubeOrient::AgeStages:
	{
		const int32 Stage = FMath::Clamp((int32)Meta * 4 / FMath::Max<int32>(1, 1 << B.MetaBits), 0, 3);
		int16 T = B.TexAlt[Stage];
		for (int32 k = Stage; T < 0 && k >= 0; --k) T = B.TexAlt[k];
		if (T >= 0) for (int32 i = 0; i < 6; ++i) OutTex[i] = T;
		break;
	}
	case EMCCubeOrient::LitToggle:
		if (MCMeta::Bit(Meta, 0))
		{
			for (int32 i = 0; i < 6; ++i)
			{
				const int16 Alt = B.TexAlt[0] >= 0 ? B.TexAlt[0] : B.Tex[i];
				OutTex[i] = Alt;
			}
		}
		break;
	default:
		break;
	}
}

void FMCBlocks::Finalize()
{
	StateInfos.Reset();
	StaticModels.Reset();
	NameToId.Reset();

	FMCState Next = 0;
	for (int32 i = 0; i < Blocks.Num(); ++i)
	{
		FMCBlock& B = Blocks[i];
		B.Id = (FMCBlockId)i;
		if (!B.Behavior) B.Behavior = &GDefaultBehavior;
		B.NumStates = (uint16)(1u << B.MetaBits);
		B.BaseState = Next;
		Next += B.NumStates;
		NameToId.Add(B.Name, B.Id);
		for (int32 f = 0; f < 6; ++f)
		{
			if (!B.TexNames[f].IsNone()) B.Tex[f] = FMCTextures::Find(B.TexNames[f]);
		}
	}
	check(Next < 65535);
	StateInfos.SetNum(Next);

	for (const FMCBlock& B : Blocks)
	{
		const bool bDynamic = B.Shape == EMCShape::Model && MCIsDynamicModel(B);
		for (uint16 M = 0; M < B.NumStates; ++M)
		{
			FMCStateInfo& SI = StateInfos[B.BaseState + M];
			SI.Block = B.Id;
			SI.Meta = (uint8)M;
			SI.Shape = B.Shape;
			SI.Layer = B.Layer;
			SI.Flags = B.Flags;
			SI.Opacity = B.LightOpacity;
			SI.Light = B.Behavior->GetLightEmission(B.BaseState + M, B.LightEmission);
			SI.bDynamicModel = bDynamic;
			ResolveCubeTextures(B, (uint8)M, SI.Tex, SI.UVRot);
			SI.bNoCollision = !(B.Flags & MCB_Solid);
			SI.bFullCollision = (B.Flags & MCB_Solid) && (B.Shape == EMCShape::Cube || B.Shape == EMCShape::Invisible);

			if (B.Shape == EMCShape::Model && !bDynamic)
			{
				FMCBlockModel Model;
				MCBuildBlockModel(B, (uint8)M, nullptr, FMCBlockPos(), Model);
				SI.StaticModel = StaticModels.Add(MoveTemp(Model));
			}
		}
	}

	auto S = [](const TCHAR* N) { return FindState(FName(N)); };
	auto Id = [](const TCHAR* N) { return FindId(FName(N)); };
	C.Air = 0; C.AirId = 0;
	C.Stone = S(TEXT("stone")); C.Dirt = S(TEXT("dirt")); C.Grass = S(TEXT("grass_block")); C.Sand = S(TEXT("sand"));
	C.RedSand = S(TEXT("red_sand")); C.Gravel = S(TEXT("gravel")); C.Water = S(TEXT("water")); C.Lava = S(TEXT("lava"));
	C.Bedrock = S(TEXT("bedrock")); C.Deepslate = S(TEXT("deepslate")); C.Cobblestone = S(TEXT("cobblestone"));
	C.Netherrack = S(TEXT("netherrack")); C.EndStone = S(TEXT("end_stone")); C.Obsidian = S(TEXT("obsidian"));
	C.Glass = S(TEXT("glass")); C.Snow = S(TEXT("snow")); C.SnowBlock = S(TEXT("snow_block")); C.Ice = S(TEXT("ice"));
	C.Clay = S(TEXT("clay")); C.Sandstone = S(TEXT("sandstone")); C.RedSandstone = S(TEXT("red_sandstone"));
	C.Terracotta = S(TEXT("terracotta")); C.Podzol = S(TEXT("podzol")); C.Mycelium = S(TEXT("mycelium"));
	C.CoarseDirt = S(TEXT("coarse_dirt")); C.Mud = S(TEXT("mud")); C.Calcite = S(TEXT("calcite")); C.Tuff = S(TEXT("tuff"));
	C.PackedIce = S(TEXT("packed_ice")); C.BlueIce = S(TEXT("blue_ice")); C.SoulSand = S(TEXT("soul_sand"));
	C.SoulSoil = S(TEXT("soul_soil")); C.Basalt = S(TEXT("basalt")); C.Blackstone = S(TEXT("blackstone"));
	C.Magma = S(TEXT("magma_block")); C.Glowstone = S(TEXT("glowstone")); C.CrimsonNylium = S(TEXT("crimson_nylium"));
	C.WarpedNylium = S(TEXT("warped_nylium")); C.Fire = S(TEXT("fire")); C.SoulFire = S(TEXT("soul_fire"));
	C.NetherPortal = S(TEXT("nether_portal")); C.EndPortal = S(TEXT("end_portal")); C.Farmland = S(TEXT("farmland"));
	C.PowderSnow = S(TEXT("powder_snow")); C.MossBlock = S(TEXT("moss_block")); C.Sulfur = S(TEXT("sulfur"));
	C.Cinnabar = S(TEXT("cinnabar")); C.PotentSulfur = S(TEXT("potent_sulfur"));
	C.WaterId = Id(TEXT("water")); C.LavaId = Id(TEXT("lava")); C.FireId = Id(TEXT("fire"));
	C.NetherPortalId = Id(TEXT("nether_portal")); C.TNTId = Id(TEXT("tnt")); C.GrassId = Id(TEXT("grass_block"));
	C.DirtId = Id(TEXT("dirt")); C.FarmlandId = Id(TEXT("farmland"));
}

const FMCBlockModel* FMCBlocks::GetStaticModel(FMCState S)
{
	const int32 Idx = StateInfos[S].StaticModel;
	return StaticModels.IsValidIndex(Idx) ? &StaticModels[Idx] : nullptr;
}

void FMCBlocks::BuildModel(FMCState S, const FMCBlockGetter* Getter, const FMCBlockPos& Pos, FMCBlockModel& Out)
{
	const FMCStateInfo& SI = StateInfos[S];
	if (SI.StaticModel >= 0)
	{
		Out = StaticModels[SI.StaticModel];
		return;
	}
	const FMCBlock& B = Blocks[SI.Block];
	MCBuildBlockModel(B, SI.Meta, Getter, Pos, Out);
}

void FMCBlocks::GetCollision(FMCState S, const FMCBlockGetter* Getter, const FMCBlockPos& Pos, TArray<FMCBox>& Out)
{
	const FMCStateInfo& SI = StateInfos[S];
	if (SI.bNoCollision) return;
	if (SI.bFullCollision) { Out.Add(FMCBox(0, 0, 0, 1, 1, 1)); return; }
	if (SI.Shape == EMCShape::Model)
	{
		if (SI.StaticModel >= 0)
		{
			const FMCBlockModel& M = StaticModels[SI.StaticModel];
			if (M.Collision.Num() > 0 || M.bExplicitCollision) { Out.Append(M.Collision); return; }
			for (const FMCModelBox& Bx : M.Boxes) Out.Add(Bx.ToBox());
			return;
		}
		FMCBlockModel M;
		MCBuildBlockModel(Blocks[SI.Block], SI.Meta, Getter, Pos, M);
		if (M.Collision.Num() > 0 || M.bExplicitCollision) { Out.Append(M.Collision); return; }
		for (const FMCModelBox& Bx : M.Boxes) Out.Add(Bx.ToBox());
		return;
	}
	Out.Add(FMCBox(0, 0, 0, 1, 1, 1));
}

void FMCBlocks::GetOutline(FMCState S, const FMCBlockGetter* Getter, const FMCBlockPos& Pos, TArray<FMCBox>& Out)
{
	const FMCStateInfo& SI = StateInfos[S];
	switch (SI.Shape)
	{
	case EMCShape::Air: return;
	case EMCShape::Liquid: Out.Add(FMCBox(0, 0, 0, 1, 1, 0.875)); return;
	case EMCShape::Cross: Out.Add(FMCBox(0.15, 0.15, 0, 0.85, 0.85, 0.8)); return;
	case EMCShape::Crop: Out.Add(FMCBox(0, 0, 0, 1, 1, 0.25 + 0.1 * (SI.Meta & 7))); return;
	case EMCShape::Model:
	{
		FMCBlockModel M;
		BuildModel(S, Getter, Pos, M);
		if (M.Outline.Num() > 0) { Out.Append(M.Outline); return; }
		if (M.Collision.Num() > 0) { Out.Append(M.Collision); return; }
		for (const FMCModelBox& Bx : M.Boxes) Out.Add(Bx.ToBox());
		if (Out.Num() == 0) Out.Add(FMCBox(0.25, 0.25, 0, 0.75, 0.75, 0.5));
		return;
	}
	default:
		Out.Add(FMCBox(0, 0, 0, 1, 1, 1));
	}
}
