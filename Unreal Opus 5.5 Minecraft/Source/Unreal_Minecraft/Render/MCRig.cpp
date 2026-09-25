// Rig registry (mob/player body plans as hierarchies of rigid parts) and the procedural pose evaluator.
#include "Render/MCRig.h"
#include "Render/MCIcons.h"
#include "Render/MCChunkMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2DArray.h"
#include "Items/MCItems.h"
#include "Blocks/MCBlocks.h"
#include "Blocks/MCTextures.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/Paths.h"
#include "AssetRegistry/AssetRegistryModule.h"

namespace
{
	TArray<FMCRigDef> GRigs;
	TMap<FName, int32> GRigIndex;
	bool GRigsInit = false;

	/** Engine cube used as the fallback box for every part. */
	UStaticMesh* GetBoxMesh()
	{
		static TWeakObjectPtr<UStaticMesh> Cached;
		if (!Cached.IsValid())
		{
			Cached = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		}
		return Cached.Get();
	}

	UMaterialInterface* LoadVoxelMaterial(const TCHAR* Path)
	{
		return LoadObject<UMaterialInterface>(nullptr, Path);
	}

	// ------------------------------------------------------------------ generated rig table
	// Tools/content/rig_defs.py is the source of truth (it also feeds the Blender builder);
	// Tools/content/gen_rigs.py writes MCRigTable.inl from it.

	struct FMCRigPartSpec
	{
		const TCHAR* Name;
		int32 Parent;
		EMCRigRole Role;
		float Pivot[3];      // relative to the parent part's pivot
		float BoxMin[3];     // relative to this part's pivot
		float BoxMax[3];
		int32 Index;
		uint32 Color;        // 0xRRGGBB, used by the box fallback
		bool bEmissive;
	};

	struct FMCRigSpec
	{
		const TCHAR* Id;
		float Scale;
		int32 HeadPart, RightArm, LeftArm;
		float HandOffset[3];
		int32 FirstPart, NumParts;
	};

#include "MCRigTable.inl"

	void InitRigs()
	{
		for (const FMCRigSpec& Spec : GRigSpecs)
		{
			FMCRigDef R;
			R.Id = FName(Spec.Id);
			R.Scale = Spec.Scale;
			R.HeadPart = Spec.HeadPart;
			R.RightArm = Spec.RightArm;
			R.LeftArm = Spec.LeftArm;
			R.HandOffset = FVector3f(Spec.HandOffset[0], Spec.HandOffset[1], Spec.HandOffset[2]);
			for (int32 i = 0; i < Spec.NumParts; ++i)
			{
				const FMCRigPartSpec& S = GRigPartSpecs[Spec.FirstPart + i];
				FMCRigPart P;
				P.Name = FName(S.Name);
				P.Parent = S.Parent;
				P.Role = S.Role;
				P.Index = S.Index;
				P.Pivot = FVector3f(S.Pivot[0], S.Pivot[1], S.Pivot[2]);
				P.BoxMin = FVector3f(S.BoxMin[0], S.BoxMin[1], S.BoxMin[2]);
				P.BoxMax = FVector3f(S.BoxMax[0], S.BoxMax[1], S.BoxMax[2]);
				P.Color = FColor((S.Color >> 16) & 255, (S.Color >> 8) & 255, S.Color & 255);
				P.bEmissive = S.bEmissive;
				R.Parts.Add(P);
			}
			const int32 Idx = GRigs.Add(MoveTemp(R));
			GRigIndex.Add(GRigs[Idx].Id, Idx);
		}
	}
}
namespace MCRigs
{
	void Init()
	{
		if (GRigsInit) return;
		GRigsInit = true;
		InitRigs();
	}

	const FMCRigDef* Find(FName Id)
	{
		Init();
		const int32* Idx = GRigIndex.Find(Id);
		return Idx ? &GRigs[*Idx] : nullptr;
	}

	const TArray<FMCRigDef>& All()
	{
		Init();
		return GRigs;
	}

	FString PartMeshPath(FName Rig, FName Part)
	{
		return FString::Printf(TEXT("/Game/Opus55Minecraft/Mobs/%s/SM_%s__%s.SM_%s__%s"), *Rig.ToString(), *Rig.ToString(), *Part.ToString(), *Rig.ToString(), *Part.ToString());
	}
}

// ---------------------------------------------------------------------------------------------------------------------

UMCRigComponent::UMCRigComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bAutoActivate = true;
}

void UMCRigComponent::Clear()
{
	// components are owned by the actor and garbage collected when the rig is rebuilt
	for (USceneComponent* C : Pivots) if (C) C->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	for (UStaticMeshComponent* M : Meshes) if (M) M->SetVisibility(false);
	Pivots.Reset(); Meshes.Reset(); MIDs.Reset();
	LightFill = LightTorch = -1.f;
	Extra.Reset(); ExtraOffset.Reset();
	Rig = nullptr;
}

void UMCRigComponent::SetRig(FName InRigId, const FColor& InTint)
{
	RigId = InRigId;
	Rig = MCRigs::Find(InRigId);
	Tint = InTint;
	BaseTint = InTint;
	Build();
}

FLinearColor UMCRigComponent::PartTint(int32 PartIndex) const
{
	// the variant tint relative to the rig's own base colour (1,1,1 = unchanged)
	const FLinearColor Cur(Tint), Base(BaseTint);
	auto Ratio = [](float C, float B) { return B > 0.02f ? FMath::Clamp(C / B, 0.f, 3.f) : 1.f; };
	const FLinearColor Rel(Ratio(Cur.R, Base.R), Ratio(Cur.G, Base.G), Ratio(Cur.B, Base.B), 1.f);
	if (PartAuthored.IsValidIndex(PartIndex) && PartAuthored[PartIndex]) return Rel;
	// box fallback: the part's own colour carries the variant shift too
	if (Rig && Rig->Parts.IsValidIndex(PartIndex) && Rig->Parts[PartIndex].Color != FColor::White)
	{
		const FLinearColor P(Rig->Parts[PartIndex].Color);
		return FLinearColor(P.R * Rel.R, P.G * Rel.G, P.B * Rel.B, 1.f);
	}
	return Cur;
}

void UMCRigComponent::SetTint(const FColor& C)
{
	Tint = C;
	for (int32 i = 0; i < MIDs.Num(); ++i)
	{
		if (!MIDs[i]) continue;
		const FLinearColor T = PartTint(i);
		MIDs[i]->SetVectorParameterValue(TEXT("Tint"), T);
		MIDs[i]->SetVectorParameterValue(TEXT("Color"), T);
	}
}

void UMCRigComponent::SetHurtFlash(float Amount)
{
	for (int32 i = 0; i < MIDs.Num(); ++i)
	{
		if (!MIDs[i]) continue;
		MIDs[i]->SetScalarParameterValue(TEXT("Hurt"), FMath::Clamp(Amount, 0.f, 1.f));
	}
}

void UMCRigComponent::SetLighting(float Fill, float Torch)
{
	// 1/64 steps: the day cycle and walking through torch light change these slowly, so most frames push nothing
	Fill = FMath::RoundToFloat(FMath::Clamp(Fill, 0.f, 1.f) * 64.f) / 64.f;
	Torch = FMath::RoundToFloat(FMath::Clamp(Torch, 0.f, 1.f) * 64.f) / 64.f;
	if (Fill == LightFill && Torch == LightTorch) return;
	LightFill = Fill;
	LightTorch = Torch;
	for (UMaterialInstanceDynamic* MID : MIDs)
	{
		if (!MID) continue;
		MID->SetScalarParameterValue(TEXT("Fill"), Fill);
		MID->SetScalarParameterValue(TEXT("TorchLight"), Torch);
	}
}

void UMCRigComponent::SetGlow(float Amount)
{
	for (int32 i = 0; i < MIDs.Num(); ++i)
	{
		if (!MIDs[i]) continue;
		MIDs[i]->SetScalarParameterValue(TEXT("Glow"), FMath::Clamp(Amount, 0.f, 1.f));
	}
}

void UMCRigComponent::SetHidden(bool bHide)
{
	SetVisibility(!bHide, true);
}

void UMCRigComponent::SetPartVisible(FName Part, bool bShow)
{
	if (Rig)
	{
		const int32 Idx = Rig->FindPart(Part);
		if (Pivots.IsValidIndex(Idx) && Pivots[Idx]) Pivots[Idx]->SetVisibility(bShow, true);
	}
}

void UMCRigComponent::AttachToPart(USceneComponent* Child, FName Part, const FVector& OffsetBlocks, const FRotator& Rot)
{
	if (!Rig || !Child) return;
	const int32 Idx = Rig->FindPart(Part);
	if (!Pivots.IsValidIndex(Idx) || !Pivots[Idx]) return;
	Child->AttachToComponent(Pivots[Idx], FAttachmentTransformRules::KeepRelativeTransform);
	Child->SetRelativeLocation(OffsetBlocks * MC::BlockSizeF);
	Child->SetRelativeRotation(Rot);
}

USceneComponent* UMCRigComponent::GetPart(FName Part) const
{
	if (!Rig) return nullptr;
	const int32 Idx = Rig->FindPart(Part);
	return Pivots.IsValidIndex(Idx) ? Pivots[Idx].Get() : nullptr;
}

USceneComponent* UMCRigComponent::GetPartByRole(EMCRigRole Role, int32 Index) const
{
	if (!Rig) return nullptr;
	for (int32 i = 0; i < Rig->Parts.Num(); ++i)
		if (Rig->Parts[i].Role == Role && (Index < 0 || Rig->Parts[i].Index == Index)) return Pivots.IsValidIndex(i) ? Pivots[i].Get() : nullptr;
	return nullptr;
}

void UMCRigComponent::Build()
{
	// Clear() forgets the definition too, so keep the one SetRig just looked up
	const FMCRigDef* Def = Rig;
	Clear();
	Rig = Def;
	if (!Rig) return;
	UStaticMesh* Box = GetBoxMesh();
	UMaterialInterface* Mat = LoadVoxelMaterial(TEXT("/Game/Opus55Minecraft/Materials/M_MCEntity.M_MCEntity"));
	if (!Mat) Mat = LoadVoxelMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Mat) Mat = LoadVoxelMaterial(TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));

	const int32 N = Rig->Parts.Num();
	Pivots.SetNum(N); Meshes.SetNum(N); MIDs.SetNum(N);
	Extra.SetNum(N); ExtraOffset.SetNum(N);
	PartAuthored.Init(false, N);
	ModelScale = Rig->Scale;

	for (int32 i = 0; i < N; ++i)
	{
		const FMCRigPart& P = Rig->Parts[i];
		// authored mesh first
		UStaticMesh* PartMesh = LoadObject<UStaticMesh>(nullptr, *MCRigs::PartMeshPath(Rig->Id, P.Name));
		USceneComponent* Parent = P.Parent >= 0 && Pivots.IsValidIndex(P.Parent) ? Pivots[P.Parent].Get() : this;
		// authored parts are modelled around their pivot, so the mesh itself is the joint; only the box fallback
		// (offset and scaled cube) needs a separate pivot. Half the components to move every frame.
		USceneComponent* Pivot = nullptr;
		if (!PartMesh)
		{
			Pivot = NewObject<USceneComponent>(GetOwner(), *FString::Printf(TEXT("Pivot_%s"), *P.Name.ToString()));
			Pivot->SetupAttachment(Parent);
			Pivot->RegisterComponent();
		}

		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(GetOwner(), *FString::Printf(TEXT("Part_%s"), *P.Name.ToString()));
		Mesh->SetupAttachment(Pivot ? Pivot : Parent);
		if (!Pivot) Pivot = Mesh;
		Pivots[i] = Pivot;
		Mesh->SetStaticMesh(PartMesh ? PartMesh : Box);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(true);
		Mesh->bReceivesDecals = false;
		Mesh->SetMobility(EComponentMobility::Movable);
		// dozens of small animated parts per mob: keep them out of the ray-tracing scene and distance-field
		// lighting (Lumen still lights them from screen space), and stop drawing past the entity range
		Mesh->bVisibleInRayTracing = false;
		Mesh->bAffectDistanceFieldLighting = false;
		Mesh->bAffectDynamicIndirectLighting = false;
		Mesh->SetCachedMaxDrawDistance(Rig->Scale > 2.f ? 25600.f : 8000.f);
		PartAuthored[i] = PartMesh != nullptr;
		if (Mat)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, Mesh);
			const FLinearColor C = PartTint(i);
			MID->SetVectorParameterValue(TEXT("Tint"), C);
			MID->SetVectorParameterValue(TEXT("Color"), C);
			MID->SetScalarParameterValue(TEXT("Part"), (float)i);
			// eyes, blaze rods, warden tendrils: self-lit parts
			MID->SetScalarParameterValue(TEXT("Emissive"), P.bEmissive ? 0.6f : 0.f);
			Mesh->SetMaterial(0, MID);
			MIDs[i] = MID;
		}
		// box fallback: unit cube (1x1x1, origin at centre) scaled and offset so it spans BoxMin..BoxMax
		const float SX = P.BoxMax.X - P.BoxMin.X, SY = P.BoxMax.Y - P.BoxMin.Y, SZ = P.BoxMax.Z - P.BoxMin.Z;
		Mesh->SetRelativeScale3D(PartMesh ? FVector(1.f) : FVector(FMath::Max(SX, 0.001f), FMath::Max(SY, 0.001f), FMath::Max(SZ, 0.001f)));
		const FVector Centre((P.BoxMin.X + P.BoxMax.X) * 0.5f, (P.BoxMin.Y + P.BoxMax.Y) * 0.5f, (P.BoxMin.Z + P.BoxMax.Z) * 0.5f);
		if (!PartMesh) Mesh->SetRelativeLocation(Centre * MC::BlockSizeF);
		Mesh->RegisterComponent();
		Meshes[i] = Mesh;

		Pivot->SetRelativeLocation(FVector(P.Pivot) * MC::BlockSizeF);
		Extra[i] = FRotator::ZeroRotator;
		ExtraOffset[i] = FVector::ZeroVector;
	}
}

void UMCRigComponent::SetPartRotation(int32 PartIndex, const FRotator& R)
{
	if (Extra.IsValidIndex(PartIndex)) Extra[PartIndex] = R;
}

void UMCRigComponent::SetPartOffset(int32 PartIndex, const FVector& OffsetBlocks)
{
	if (ExtraOffset.IsValidIndex(PartIndex)) ExtraOffset[PartIndex] = OffsetBlocks * MC::BlockSizeF;
}

/** Minecraft-ish limb angle helper. */
static float LimbAngle(float Swing, float Amplitude, float Offset = 1.f)
{
	return FMath::Cos(Swing * 0.6662f + (Offset > 0 ? PI : 0.f)) * 1.4f * Amplitude;
}

void UMCRigComponent::ApplyPose(const FMCRigPose& Pose)
{
	if (!Rig) return;
	const float DeathTilt = Pose.Death > 0.f ? 90.f * FMath::Min(1.f, Pose.Death) : 0.f;
	const float BabyScale = Pose.bBaby ? 0.6f : 1.f;
	const float SitOffset = Pose.bSitting ? -0.25f : 0.f;
	const float SwimOffset = Pose.bSwimming ? 0.35f : 0.f;
	SetRelativeScale3D(FVector(ModelScale * BabyScale));

	for (int32 i = 0; i < Rig->Parts.Num() && i < Pivots.Num(); ++i)
	{
		const FMCRigPart& P = Rig->Parts[i];
		USceneComponent* Pivot = Pivots[i];
		if (!Pivot) continue;
		FRotator Rot = Extra[i];
		FVector Off = ExtraOffset[i];
		const float S = Pose.Special;
		switch (P.Role)
		{
		case EMCRigRole::Head:
		{
			Rot += FRotator(-FMath::Clamp(Pose.HeadPitch, -90.f, 90.f), FMath::Clamp(Pose.HeadYaw, -85.f, 85.f), 0.f);
			if (Pose.bSitting) Off += FVector(0, 0, SitOffset * MC::BlockSizeF);
			if (Pose.bSleeping) Rot += FRotator(0, 0, 90.f);
			break;
		}
		case EMCRigRole::Jaw:
			Rot += FRotator(FMath::Clamp(S * 30.f, 0.f, 30.f), 0, 0);
			break;
		case EMCRigRole::Neck:
			Rot += FRotator(-FMath::Clamp(Pose.HeadPitch, -40.f, 40.f) * 0.3f + FMath::Sin(Pose.Age * 0.05f + P.Index) * 3.f, FMath::Clamp(Pose.HeadYaw, -40.f, 40.f) * 0.25f, 0);
			break;
		case EMCRigRole::ArmR:
			Rot += FRotator(-LimbAngle(Pose.LimbSwing, Pose.LimbAmount, 1.f) * 57.2958f * 0.6f, 0, 0);
			Rot += FRotator(-Pose.Attack * 100.f, 0, 0);
			if (Pose.bBlocking) Rot += FRotator(-90.f, -35.f, 0);
			if (Pose.bSwimming) Rot += FRotator(-160.f, 0, 0);
			if (Pose.bAggressive) Rot += FRotator(-70.f, 0, -10.f);
			break;
		case EMCRigRole::ArmL:
			Rot += FRotator(LimbAngle(Pose.LimbSwing, Pose.LimbAmount, -1.f) * 57.2958f * 0.6f, 0, 0);
			Rot += FRotator(-Pose.Attack * 100.f, 0, 0);
			if (Pose.bSwimming) Rot += FRotator(-160.f, 0, 0);
			break;
		case EMCRigRole::LegFR:
			Rot += FRotator(LimbAngle(Pose.LimbSwing, Pose.LimbAmount, -1.f) * 57.2958f * 0.7f, 0, 0);
			break;
		case EMCRigRole::LegFL:
			Rot += FRotator(LimbAngle(Pose.LimbSwing, Pose.LimbAmount, 1.f) * 57.2958f * 0.7f, 0, 0);
			break;
		case EMCRigRole::LegBR:
			Rot += FRotator(LimbAngle(Pose.LimbSwing, Pose.LimbAmount, 1.f) * 57.2958f * 0.7f, 0, 0);
			break;
		case EMCRigRole::LegBL:
			Rot += FRotator(LimbAngle(Pose.LimbSwing, Pose.LimbAmount, -1.f) * 57.2958f * 0.7f, 0, 0);
			break;
		case EMCRigRole::LegMR:
			Rot += FRotator(0, 0, -FMath::Abs(FMath::Sin(Pose.LimbSwing * 0.6662f + P.Index * 0.8f)) * 0.6f * Pose.LimbAmount * 57.2958f + 20.f);
			Rot += FRotator(FMath::Cos(Pose.LimbSwing * 0.6662f + P.Index * 0.8f) * 0.4f * Pose.LimbAmount * 57.2958f, 0, 0);
			break;
		case EMCRigRole::LegML:
			Rot += FRotator(0, 0, FMath::Abs(FMath::Sin(Pose.LimbSwing * 0.6662f + P.Index * 0.8f)) * 0.6f * Pose.LimbAmount * 57.2958f - 20.f);
			Rot += FRotator(FMath::Cos(Pose.LimbSwing * 0.6662f + P.Index * 0.8f + PI) * 0.4f * Pose.LimbAmount * 57.2958f, 0, 0);
			break;
		case EMCRigRole::WingR:
			Rot += FRotator(S * FMath::Sin(Pose.Age * 0.9f + P.Index) * 45.f, -FMath::Abs(S) * 10.f, 0);
			if (Pose.bSitting) Rot += FRotator(0, 0, 55.f);
			break;
		case EMCRigRole::WingL:
			Rot += FRotator(S * FMath::Sin(Pose.Age * 0.9f + P.Index) * 45.f, FMath::Abs(S) * 10.f, 0);
			if (Pose.bSitting) Rot += FRotator(0, 0, -55.f);
			break;
		case EMCRigRole::Tail:
			Rot += FRotator(0, FMath::Sin(Pose.Age * 0.15f + P.Index * 0.7f) * (4.f + P.Index), FMath::Sin(Pose.Age * 0.1f + P.Index) * 4.f);
			if (Pose.bSitting) Rot += FRotator(0, 0, 20.f);
			break;
		case EMCRigRole::FinR:
		case EMCRigRole::FinL:
			Rot += FRotator(0, 0, FMath::Sin(Pose.Age * 0.6f + P.Index) * 20.f + S * 20.f);
			break;
		case EMCRigRole::Rod:
			Rot += FRotator(FMath::Sin(Pose.Age * 0.2f + P.Index) * 30.f, FMath::Cos(Pose.Age * 0.25f + P.Index * 1.7f) * 30.f, 0);
			break;
		case EMCRigRole::EarL:
		case EMCRigRole::EarR:
			Rot += FRotator(FMath::Sin(Pose.Age * 0.3f + P.Index) * 8.f, 0, 0);
			break;
		case EMCRigRole::Body:
			if (Pose.bSitting) Off += FVector(0, 0, SitOffset * MC::BlockSizeF);
			if (Pose.bSwimming) Off += FVector(0, 0, SwimOffset * MC::BlockSizeF);
			if (Pose.bSleeping) Rot += FRotator(0, 0, 90.f);
			break;
		default: break;
		}
		// one transform update per joint (the engine skips it when nothing moved)
		Pivot->SetRelativeLocationAndRotation(FVector(P.Pivot) * MC::BlockSizeF + Off, Rot);
	}
}

bool UMCRigComponent::IsOnScreen(float Tolerance) const
{
	for (const UStaticMeshComponent* M : Meshes)
		if (M && M->WasRecentlyRendered(Tolerance)) return true;
	return false;
}



// ---------------------------------------------------------------------------------------------------------------------

UMCItemVisualComponent::UMCItemVisualComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	Authored = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Authored"));
	if (Authored)
	{
		Authored->SetupAttachment(this);
		Authored->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Authored->SetMobility(EComponentMobility::Movable);
		Authored->SetVisibility(false);
	}
	Generated = CreateDefaultSubobject<UMCChunkMeshComponent>(TEXT("Generated"));
	if (Generated)
	{
		Generated->bRetainData = true;
		Generated->SetupAttachment(this);
		Generated->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Generated->SetMobility(EComponentMobility::Movable);
	}
}

void UMCItemVisualComponent::OnRegister()
{
	Super::OnRegister();
	// the constructor's children are registered together with their actor only when that actor spawns; visuals
	// created at runtime (first-person hand, mob-held items) have to register them themselves
	if (!GetOwner() || !GetWorld()) return;
	if (Authored && !Authored->IsRegistered()) { Authored->SetupAttachment(this); Authored->RegisterComponent(); }
	if (Generated && !Generated->IsRegistered()) { Generated->SetupAttachment(this); Generated->RegisterComponent(); }
}

TUniquePtr<FMCChunkMeshData> UMCItemVisualComponent::BuildIconMesh(int32 IconLayer, int32 Resolution, float Thickness)
{
	// Minecraft-style extruded sprite: the sprite on a front and a back face (alpha-tested), plus a side wall along
	// every edge between an opaque and a transparent texel, so the item reads as a solid 3D prop from any angle.
	TUniquePtr<FMCChunkMeshData> Data = MakeUnique<FMCChunkMeshData>();
	FMCMeshLayerData& L = Data->Layers[(int32)EMCLayer::Cutout];
	// UU like every other chunk mesh (1 icon = 1 block wide before the component scale)
	const float H = 0.5f * MC::BlockSizeF;
	const float T = Thickness * 0.5f * MC::BlockSizeF;
	auto V = [&](const FVector3f& Pos, const FVector3f& N, const FVector2f& UV0)
	{
		FMCMeshVertex Vert;
		Vert.Pos = Pos; Vert.Normal = N;
		Vert.Tangent = FMath::Abs(N.Z) > 0.5f ? FVector3f(1, 0, 0) : FVector3f(0, 0, 1);
		Vert.UV0 = UV0;
		Vert.UV1 = FVector2f((float)IconLayer, (float)MCRender::VF_Tint);
		Vert.UV2 = FVector2f(1.f, 1.f);
		Vert.Color = FColor::White;
		L.Vertices.Add(Vert);
	};
	auto Quad = [&](const FVector3f& A, const FVector3f& B, const FVector3f& C, const FVector3f& D, const FVector3f& N,
		const FVector2f& UA, const FVector2f& UB, const FVector2f& UC, const FVector2f& UD)
	{
		const int32 Base = L.Vertices.Num();
		V(A, N, UA); V(B, N, UB); V(C, N, UC); V(D, N, UD);
		L.Indices.Add(Base + 0); L.Indices.Add(Base + 1); L.Indices.Add(Base + 2);
		L.Indices.Add(Base + 0); L.Indices.Add(Base + 2); L.Indices.Add(Base + 3);
	};
	// faces: X = image columns (left to right), Z = image rows (row 0 at the top), front faces -Y
	Quad(FVector3f(-H, -T, -H), FVector3f(H, -T, -H), FVector3f(H, -T, H), FVector3f(-H, -T, H), FVector3f(0, -1, 0),
		FVector2f(0, 1), FVector2f(1, 1), FVector2f(1, 0), FVector2f(0, 0));
	Quad(FVector3f(H, T, -H), FVector3f(-H, T, -H), FVector3f(-H, T, H), FVector3f(H, T, H), FVector3f(0, 1, 0),
		FVector2f(1, 1), FVector2f(0, 1), FVector2f(0, 0), FVector2f(1, 0));

	const FColor* Px = MCIcons::SpritePixels(IconLayer);
	const int32 S = MCIcons::SpriteSize;
	auto Solid = [Px, S](int32 X, int32 Y) { return X >= 0 && Y >= 0 && X < S && Y < S && Px[Y * S + X].A >= 128; };
	if (Px)
	{
		const float P = 2.f * H / S;
		for (int32 y = 0; y < S; ++y)
		{
			for (int32 x = 0; x < S; ++x)
			{
				if (!Solid(x, y)) continue;
				const float X0 = -H + x * P, X1 = X0 + P;
				const float Z1 = H - y * P, Z0 = Z1 - P;
				// every vertex of a wall samples the centre of its texel, so the wall takes that texel's colour
				const FVector2f UV((x + 0.5f) / S, (y + 0.5f) / S);
				if (!Solid(x - 1, y)) Quad(FVector3f(X0, T, Z0), FVector3f(X0, -T, Z0), FVector3f(X0, -T, Z1), FVector3f(X0, T, Z1), FVector3f(-1, 0, 0), UV, UV, UV, UV);
				if (!Solid(x + 1, y)) Quad(FVector3f(X1, -T, Z0), FVector3f(X1, T, Z0), FVector3f(X1, T, Z1), FVector3f(X1, -T, Z1), FVector3f(1, 0, 0), UV, UV, UV, UV);
				if (!Solid(x, y - 1)) Quad(FVector3f(X0, -T, Z1), FVector3f(X1, -T, Z1), FVector3f(X1, T, Z1), FVector3f(X0, T, Z1), FVector3f(0, 0, 1), UV, UV, UV, UV);
				if (!Solid(x, y + 1)) Quad(FVector3f(X0, T, Z0), FVector3f(X1, T, Z0), FVector3f(X1, -T, Z0), FVector3f(X0, -T, Z0), FVector3f(0, 0, -1), UV, UV, UV, UV);
			}
		}
	}
	(void)Resolution;
	Data->Bounds = FBox(FVector(-H, -T, -H), FVector(H, T, H));
	return Data;
}

TUniquePtr<FMCChunkMeshData> UMCItemVisualComponent::BuildBlockMesh(uint16 State, float Size)
{
	TUniquePtr<FMCChunkMeshData> Data = MakeUnique<FMCChunkMeshData>();
	FMCBlockModel Model;
	FMCBlocks::BuildModel(State, nullptr, FMCBlockPos(0, 0, 0), Model);
	const float S = Size * MC::BlockSizeF;   // Size in blocks, vertices in UU
	auto Layer = [&](int32 L) -> FMCMeshLayerData& { return Data->Layers[FMath::Clamp(L, 0, FMCChunkMeshData::NumLayers - 1)]; };
	const EMCLayer DefaultLayer = FMCBlocks::Info(State).Layer;
	for (const FMCModelBox& B : Model.Boxes)
	{
		for (int32 F = 0; F < 6; ++F)
		{
			if (B.Tex[F] < 0) continue;
			const float X0 = B.Min.X * S - S * 0.5f, X1 = B.Max.X * S - S * 0.5f;
			const float Y0 = B.Min.Y * S - S * 0.5f, Y1 = B.Max.Y * S - S * 0.5f;
			const float Z0 = B.Min.Z * S - S * 0.5f, Z1 = B.Max.Z * S - S * 0.5f;
			FVector3f P[4];
			switch ((EMCFace)F)
			{
			case EMCFace::Down:  P[0] = FVector3f(X0, Y1, Z0); P[1] = FVector3f(X1, Y1, Z0); P[2] = FVector3f(X1, Y0, Z0); P[3] = FVector3f(X0, Y0, Z0); break;
			case EMCFace::Up:    P[0] = FVector3f(X0, Y0, Z1); P[1] = FVector3f(X1, Y0, Z1); P[2] = FVector3f(X1, Y1, Z1); P[3] = FVector3f(X0, Y1, Z1); break;
			case EMCFace::North: P[0] = FVector3f(X1, Y0, Z0); P[1] = FVector3f(X0, Y0, Z0); P[2] = FVector3f(X0, Y0, Z1); P[3] = FVector3f(X1, Y0, Z1); break;
			case EMCFace::South: P[0] = FVector3f(X0, Y1, Z0); P[1] = FVector3f(X1, Y1, Z0); P[2] = FVector3f(X1, Y1, Z1); P[3] = FVector3f(X0, Y1, Z1); break;
			case EMCFace::West:  P[0] = FVector3f(X0, Y0, Z0); P[1] = FVector3f(X0, Y1, Z0); P[2] = FVector3f(X0, Y1, Z1); P[3] = FVector3f(X0, Y0, Z1); break;
			default:             P[0] = FVector3f(X1, Y1, Z0); P[1] = FVector3f(X1, Y0, Z0); P[2] = FVector3f(X1, Y0, Z1); P[3] = FVector3f(X1, Y1, Z1); break;
			}
			FMCMeshLayerData& LD = Layer(B.Layer == 255 ? (int32)DefaultLayer : (int32)B.Layer);
			const int32 Base = LD.Vertices.Num();
			const FVector4f UV = B.UV[F];
			// side faces: corners 0-1 are the bottom edge, so they take the texture's bottom row (v1)
			const bool bSide = F != (int32)EMCFace::Up && F != (int32)EMCFace::Down;
			const float VBottom = bSide ? UV.W : UV.Y, VTop = bSide ? UV.Y : UV.W;
			const FVector2f UVs[4] = { FVector2f(UV.X, VBottom), FVector2f(UV.Z, VBottom), FVector2f(UV.Z, VTop), FVector2f(UV.X, VTop) };
			// items have no biome: tinted faces take the inventory colours; the opaque material applies the tint only
			// where the texture's alpha mask asks for it (grass fringe, not the dirt below)
			const FMCBlock& Blk = FMCBlocks::GetByState(State);
			const bool bTintFace = (B.TintFaces & (1 << F)) || Blk.Tint != EMCTint::None;
			FColor FaceColor = B.Color;
			if (bTintFace && FaceColor == FColor::White) FaceColor = MCIcons::ItemTint(Blk);
			for (int32 k = 0; k < 4; ++k)
			{
				FMCMeshVertex Vert;
				Vert.Pos = P[k];
				Vert.Normal = (P[2] - P[0]).Cross(P[1] - P[0]).GetSafeNormal();
				if (FMath::IsNearlyZero(Vert.Normal.SizeSquared())) Vert.Normal = FVector3f(0, 0, 1);
				Vert.Tangent = FVector3f(0, 1, 0);
				Vert.UV0 = UVs[k];
				Vert.UV1 = FVector2f((float)B.Tex[F], (float)(B.bEmissive ? MCRender::VF_Emissive : (bTintFace ? MCRender::VF_Tint : 0)));
				Vert.UV2 = FVector2f(1.f, 1.f);
				Vert.Color = FaceColor;
				LD.Vertices.Add(Vert);
			}
			LD.Indices.Add(Base + 0); LD.Indices.Add(Base + 1); LD.Indices.Add(Base + 2);
			LD.Indices.Add(Base + 0); LD.Indices.Add(Base + 2); LD.Indices.Add(Base + 3);
			if (B.bDoubleSided)
			{
				const int32 Base2 = LD.Vertices.Num();
				for (int32 k = 3; k >= 0; --k) { FMCMeshVertex Vert = LD.Vertices[Base + k]; Vert.Normal = -Vert.Normal; LD.Vertices.Add(Vert); }
				LD.Indices.Add(Base2 + 0); LD.Indices.Add(Base2 + 1); LD.Indices.Add(Base2 + 2);
				LD.Indices.Add(Base2 + 0); LD.Indices.Add(Base2 + 2); LD.Indices.Add(Base2 + 3);
			}
		}
	}
	for (const FMCModelQuad& Q : Model.Quads)
	{
		FMCMeshLayerData& LD = Layer(Q.Layer == 255 ? (int32)DefaultLayer : (int32)Q.Layer);
		const int32 Base = LD.Vertices.Num();
		for (int32 k = 0; k < 4; ++k)
		{
			FMCMeshVertex Vert;
			Vert.Pos = (Q.P[k] - FVector3f(0.5f, 0.5f, 0.5f)) * S;
			Vert.Normal = (Q.P[2] - Q.P[0]).Cross(Q.P[1] - Q.P[0]).GetSafeNormal();
			if (FMath::IsNearlyZero(Vert.Normal.SizeSquared())) Vert.Normal = FVector3f(0, 0, 1);
			Vert.Tangent = FVector3f(0, 1, 0);
			Vert.UV0 = Q.UV[k];
			Vert.UV1 = FVector2f((float)Q.Tex, (float)(Q.bEmissive ? MCRender::VF_Emissive : (Q.bTint ? MCRender::VF_Tint : 0)));
			Vert.UV2 = FVector2f(1.f, 1.f);
			Vert.Color = Q.Color;
			LD.Vertices.Add(Vert);
		}
		LD.Indices.Add(Base + 0); LD.Indices.Add(Base + 1); LD.Indices.Add(Base + 2);
		LD.Indices.Add(Base + 0); LD.Indices.Add(Base + 2); LD.Indices.Add(Base + 3);
		if (Q.bDoubleSided)
		{
			const int32 Base2 = LD.Vertices.Num();
			for (int32 k = 3; k >= 0; --k) { FMCMeshVertex Vert = LD.Vertices[Base + k]; Vert.Normal = -Vert.Normal; LD.Vertices.Add(Vert); }
			LD.Indices.Add(Base2 + 0); LD.Indices.Add(Base2 + 1); LD.Indices.Add(Base2 + 2);
			LD.Indices.Add(Base2 + 0); LD.Indices.Add(Base2 + 2); LD.Indices.Add(Base2 + 3);
		}
	}
	Data->Bounds = FBox(FVector(-0.5 * S, -0.5 * S, -0.5 * S), FVector(0.5 * S, 0.5 * S, 0.5 * S));
	return Data;
}

namespace
{
	/** Authored held-item prop for an item (Tools/BlenderMCP/build_items.py) and its tier look. */
	struct FMCItemModel
	{
		FName Mesh;
		FLinearColor Tint = FLinearColor::White;
		float Metal = 0.f;
		float Rough = 0.6f;
	};

	bool ResolveItemModel(const FMCItem& Item, FMCItemModel& Out)
	{
		const FString N = Item.Name.ToString();
		if (!Item.MeshAsset.IsNone()) { Out.Mesh = Item.MeshAsset; return true; }
		struct FKind { const TCHAR* Suffix; const TCHAR* Mesh; };
		static const FKind Kinds[] = {
			{ TEXT("_pickaxe"), TEXT("SM_Pickaxe") }, { TEXT("_sword"), TEXT("SM_Sword") }, { TEXT("_shovel"), TEXT("SM_Shovel") },
			{ TEXT("_hoe"), TEXT("SM_Hoe") }, { TEXT("_spear"), TEXT("SM_Spear") }, { TEXT("_axe"), TEXT("SM_Axe") } };
		for (const FKind& K : Kinds)
		{
			if (!N.EndsWith(K.Suffix)) continue;
			Out.Mesh = K.Mesh;
			// the head takes the tier's colour and metal response (one mesh for wooden..netherite)
			struct FTier { const TCHAR* Prefix; FColor Color; float Metal; float Rough; };
			static const FTier Tiers[] = {
				{ TEXT("wooden_"), FColor(162, 124, 76), 0.f, 0.72f }, { TEXT("stone_"), FColor(136, 136, 134), 0.f, 0.85f },
				{ TEXT("copper_"), FColor(206, 112, 76), 0.9f, 0.34f }, { TEXT("iron_"), FColor(226, 226, 230), 0.95f, 0.3f },
				{ TEXT("golden_"), FColor(252, 206, 64), 1.f, 0.22f }, { TEXT("diamond_"), FColor(96, 236, 226), 0.15f, 0.1f },
				{ TEXT("netherite_"), FColor(82, 70, 76), 0.9f, 0.36f } };
			for (const FTier& T : Tiers)
				if (N.StartsWith(T.Prefix)) { Out.Tint = FLinearColor(T.Color); Out.Metal = T.Metal; Out.Rough = T.Rough; }
			return true;
		}
		if (N == TEXT("mace")) { Out.Mesh = TEXT("SM_Mace"); Out.Metal = 0.85f; Out.Rough = 0.4f; return true; }
		if (N == TEXT("trident")) { Out.Mesh = TEXT("SM_Trident"); Out.Metal = 0.6f; Out.Rough = 0.3f; return true; }
		if (N == TEXT("bow")) { Out.Mesh = TEXT("SM_Bow"); return true; }
		if (N == TEXT("crossbow")) { Out.Mesh = TEXT("SM_Crossbow"); Out.Metal = 0.8f; Out.Rough = 0.45f; return true; }
		if (N == TEXT("shield")) { Out.Mesh = TEXT("SM_Shield"); Out.Metal = 0.8f; Out.Rough = 0.45f; return true; }
		if (N == TEXT("fishing_rod")) { Out.Mesh = TEXT("SM_FishingRod"); Out.Metal = 0.7f; Out.Rough = 0.4f; return true; }
		return false;
	}
}

void UMCItemVisualComponent::SetStack(const FMCItemStack& Stack, int32 Mode)
{
	if (Stack.Id == CurrentId && Mode == CurrentMode && Stack.Damage == 0) return;
	CurrentMode = Mode;
	CurrentId = Stack.Id;
	bIsBlock = false;
	CurrentState = 0;
	ItemName = NAME_None;
	// drop the previous geometry, not just hide it: owners propagate visibility to every child each frame
	if (Generated) { Generated->ClearMesh(); Generated->SetVisibility(false); }
	if (Authored) { Authored->SetStaticMesh(nullptr); Authored->SetVisibility(false); }
	if (Stack.IsEmpty()) return;
	const FMCItem& Item = Stack.Item();
	ItemName = Item.Name;
	// sizes in blocks per display mode: 0 dropped (entity scales it again), 1 first person, 2 held by a mob, 3 item frame
	static const float IconScale[4] = { 1.f, 0.23f, 1.f, 0.5f };
	const int32 ModeIndex = FMath::Clamp(Mode, 0, 3);
	// authored Blender prop (tools, weapons, bow, shield...), same item-space layout as the sprites
	FMCItemModel Model;
	if (Authored && ResolveItemModel(Item, Model))
	{
		const FString Name = Model.Mesh.ToString();
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Opus55Minecraft/Items/%s.%s"), *Name, *Name)))
		{
			Authored->SetStaticMesh(Mesh);
			// one entity-material instance per slot: "MI_Metal" slots take the tier's metal response
			UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Opus55Minecraft/Materials/M_MCEntity.M_MCEntity"));
			const TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
			for (int32 i = 0; Base && i < Slots.Num(); ++i)
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
				const bool bMetal = Slots[i].MaterialSlotName.ToString().Contains(TEXT("Metal"));
				MID->SetVectorParameterValue(TEXT("Tint"), Model.Tint);
				MID->SetVectorParameterValue(TEXT("Color"), Model.Tint);
				MID->SetScalarParameterValue(TEXT("Metallic"), bMetal ? Model.Metal : 0.f);
				MID->SetScalarParameterValue(TEXT("Roughness"), bMetal ? Model.Rough : 0.72f);
				MID->SetScalarParameterValue(TEXT("Emissive"), Mode == 1 ? 0.2f : 0.f);
				Authored->SetMaterial(i, MID);
			}
			Authored->SetRelativeScale3D(FVector(IconScale[ModeIndex] * (ModeIndex == 1 ? 0.85f : 1.f)));
			Authored->SetVisibility(true);
			return;
		}
	}
	// sizes in blocks per display mode: 0 dropped (entity scales it again), 1 first person, 2 held by a mob, 3 item frame
	if (Item.Block)
	{
		static const float BlockSize[4] = { 1.f, 0.095f, 1.f, 0.5f };
		SetBlockState(FMCBlocks::Get(Item.Block).BaseState, BlockSize[FMath::Clamp(Mode, 0, 3)]);
		return;
	}
	// extruded item sprite
	const int32 Layer = MCIcons::SpriteLayer(Item.Id);
	if (Generated)
	{
		Generated->SharedMaterials = MCRender::GItemMaterials ? MCRender::GItemMaterials : MCRender::GVoxelMaterials;
		TUniquePtr<FMCChunkMeshData> Mesh = BuildIconMesh(Layer, 16, 1.f / 16.f);
		Generated->SetMeshData(MoveTemp(Mesh));
		Generated->SetVisibility(true);
		Generated->SetRelativeScale3D(FVector(IconScale[ModeIndex]));
	}
}

void UMCItemVisualComponent::SetBlockState(uint16 State, float SizeBlocks)
{
	CurrentState = State;
	CurrentId = 0;
	bIsBlock = true;
	if (Authored) { Authored->SetStaticMesh(nullptr); Authored->SetVisibility(false); }
	if (Generated)
	{
		Generated->SetRelativeScale3D(FVector(1.f));
		Generated->SharedMaterials = MCRender::GVoxelMaterials;
		TUniquePtr<FMCChunkMeshData> Mesh = BuildBlockMesh(State, SizeBlocks);
		Generated->SetMeshData(MoveTemp(Mesh));
		Generated->SetVisibility(true);
	}
}

void UMCItemVisualComponent::Clear()
{
	CurrentId = 0; CurrentState = 0; CurrentMode = -1;
	if (Authored) Authored->SetVisibility(false);
	if (Generated) { Generated->ClearMesh(); Generated->SetVisibility(false); }
}

void UMCItemVisualComponent::SetHiddenAll(bool bHide)
{
	SetVisibility(!bHide, true);
}

void UMCItemVisualComponent::SetFlash(float Amount)
{
	if (Generated)
	{
		// emissive flash handled by the material parameter
		SetVisibility(true, true);
	}
}
