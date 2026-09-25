// First-person hand: arm + held item with Minecraft-style equip, swing, walk bob, eating and bow draw poses.
#include "Game/MCHeldItem.h"
#include "Game/MCPlayer.h"
#include "Render/MCRig.h"
#include "Render/MCAssets.h"
#include "Render/MCChunkMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Items/MCItems.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MCHeldItem)

UMCHeldItemComponent::UMCHeldItemComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMCHeldItemComponent::EnsureParts()
{
	AActor* Owner = GetOwner();
	if (Hand) return;
	Hand = NewObject<USceneComponent>(Owner ? (UObject*)Owner : (UObject*)this, TEXT("HandPivot"));
	Hand->SetupAttachment(this);
	Hand->RegisterComponent();

	USceneComponent* HandPtr = Hand;

	// arm: authored mesh when available, otherwise a box standing in for the forearm
	Arm = NewObject<UStaticMeshComponent>(Owner ? (UObject*)Owner : (UObject*)this, TEXT("Arm"));
	Arm->SetMobility(EComponentMobility::Movable);
	Arm->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Arm->SetCastShadow(false);
	Arm->bReceivesDecals = false;
	Arm->SetupAttachment(HandPtr);
	if (UStaticMesh* Authored = MCAssets::Mesh(TEXT("/Game/Opus55Minecraft/Entities/SM_PlayerArm.SM_PlayerArm")))
	{
		Arm->SetStaticMesh(Authored);
		Arm->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
		Arm->SetRelativeScale3D(FVector(1.f));
		bAuthoredArm = true;
	}
	else if (UStaticMesh* Cube = MCAssets::Cube())
	{
		Arm->SetStaticMesh(Cube);
		// 42 x 12 x 12 UU forearm running back along -X, shifted so the wrist sits at the origin
		Arm->SetRelativeLocation(FVector(-21.f, 0.f, 0.f));
		Arm->SetRelativeScale3D(FVector(0.42f, 0.12f, 0.12f));
	}
	// the authored arm carries skin and sleeve in its vertex colours; the stand-in box is tinted skin
	const FLinearColor ArmTint = bAuthoredArm ? FLinearColor::White : FLinearColor(0.86f, 0.65f, 0.52f);
	UMaterialInstanceDynamic* MID = MCAssets::MakeMID(Arm, MCAssets::EntityMaterial(), ArmTint);
	if (MID)
	{
		MID->SetScalarParameterValue(TEXT("Glow"), 0.f);
		MID->SetVectorParameterValue(TEXT("Tint"), ArmTint);
		ArmMID = MID;
	}
	Arm->RegisterComponent();

	if (!Item)
	{
		Item = NewObject<UMCItemVisualComponent>(Owner ? (UObject*)Owner : (UObject*)this, TEXT("HeldVisual"));
		Item->SetupAttachment(HandPtr);
		Item->RegisterComponent();
	}
}

void UMCHeldItemComponent::SetStack(const FMCItemStack& Stack)
{
	EnsureParts();
	const bool bEmpty = Stack.IsEmpty();
	if (Stack.Id == ShownId && Stack.Damage == ShownDamage && bEmpty == bShownEmpty && !bEmpty) return;
	ShownId = Stack.Id;
	ShownDamage = Stack.Damage;
	bShownEmpty = bEmpty;
	if (bEmpty)
	{
		if (Item) Item->Clear();
		if (Arm) Arm->SetVisibility(true, true);
	}
	else
	{
		if (Item) Item->SetStack(Stack, 1);
		// Java Edition shows the bare arm only when the hand is empty
		if (Arm) Arm->SetVisibility(false, true);
	}
	PrevEquipProgress = 0.f;
	EquipProgress = 0.f;
}

void UMCHeldItemComponent::SetHandVisible(bool bShow)
{
	if (Arm) Arm->SetVisibility(bShow, true);
	if (Item) Item->SetVisibility(bShow, true);
}

void UMCHeldItemComponent::UpdatePose(const AMCPlayer* P, float Alpha, float Dt)
{
	EnsureParts();
	if (!P || !Hand) return;
	PrevEquipProgress = EquipProgress;
	EquipProgress = FMath::Min(1.f, EquipProgress + Dt * 5.f);
	const float Equip = FMath::Lerp(PrevEquipProgress, EquipProgress, Alpha);
	const float EquipCurve = 1.f - FMath::Square(1.f - Equip); // ease out

	// walk bob (Minecraft: sway the hand opposite to the camera movement)
	const float Bob = P->bSprinting ? 1.f : 0.6f;
	const float BobNow = P->BobAmount * Bob;
	const float BobPrev = P->PrevBobAmount * Bob;
	const float BobT = (1.f - Alpha) * BobPrev + Alpha * BobNow;
	const float SwingPhase = P->LimbSwing;

	// use animation (eating / drinking / drawing a bow pull the item to the centre)
	float UsePush = 0.f, UsePull = 0.f;
	const bool bUsing = P->bUsingItem;
	if (bUsing)
	{
		const float UseT = P->UseItemDuration > 0 ? 1.f - (float)P->UseItemRemaining / (float)P->UseItemDuration : 0.f;
		const float Wob = FMath::Sin(FMath::Min(UseT * 6.f, 1.f) * PI);
		UsePush = Wob * 0.12f;
		UsePull = Wob * 0.5f;
	}
	// swing (mining / attacking): a quick downward arc
	const float SwingT = P->SwingTicks > 0 ? (1.f - P->SwingTicks / 6.f) : 1.f;
	const float Swing = P->SwingTicks > 0 ? FMath::Sin(SwingT * PI) : 0.f;

	// third person / other views still use the same pivot
	const float EquipX = FMath::Lerp(-0.6f, 0.f, EquipCurve);
	const float EquipY = FMath::Lerp(-0.4f, 0.f, EquipCurve);
	const float EquipRot = FMath::Lerp(-60.f, 0.f, EquipCurve);

	// block vs item offsets (Minecraft item/block held positions)
	const bool bBlock = !bShownEmpty && Item && Item->IsBlockModel();
	const float BaseX = bBlock ? 0.f : 0.f;
	const float BaseY = bBlock ? 0.f : 0.f;
	const float BaseZ = bBlock ? 0.f : 0.f;
	const FVector3f Bounce(FMath::Sin(SwingPhase) * 0.02f * BobT, -FMath::Abs(FMath::Cos(SwingPhase)) * 0.02f * BobT, 0.f);

	// camera sway from rapid mouse movement
	const float DYaw = FMath::FindDeltaAngleDegrees(LastYaw, P->Yaw);
	const float DPitch = P->Pitch - LastPitch;
	LastYaw = P->Yaw; LastPitch = P->Pitch;
	SwayYaw = FMath::FInterpTo(SwayYaw, FMath::Clamp(-DYaw * 0.6f, -6.f, 6.f), Dt, 6.f);
	SwayPitch = FMath::FInterpTo(SwayPitch, FMath::Clamp(-DPitch * 0.6f, -6.f, 6.f), Dt, 6.f);

	// placed by screen fraction 40 cm in front of the eye (lower right, like Minecraft), and scaled with the vertical
	// FOV so the hand keeps its size on screen when the FOV option or sprint/spyglass zoom changes it
	const float VFov = P->Camera ? FMath::Clamp(P->Camera->FieldOfView, 20.f, 150.f) : 70.f;
	const float K = FMath::Tan(FMath::DegreesToRadians(VFov * 0.5f)) / FMath::Tan(FMath::DegreesToRadians(35.f));
	const float Scale = (P->bSneaking ? 0.94f : 1.f) * K;
	Hand->SetRelativeLocation(FVector(40.f + (BaseX + EquipX * 0.5f + Bounce.X - UsePush) * 100.f,
		(20.f + (BaseY + EquipY * 0.3f + Bounce.Y) * 100.f) * K,
		(-11.f + (BaseZ + Bounce.Z - UsePull * 0.35f) * 100.f - Swing * 8.f) * K + EquipX * 30.f));
	Hand->SetRelativeRotation(FRotator(EquipRot * 0.5f + Swing * 42.f + SwayPitch * 0.4f, SwayYaw + Swing * -6.f, UsePull * -22.f));
	Hand->SetRelativeScale3D(FVector(Scale));
	// held pose: blocks turned 45 degrees (two faces and the top show), sprites seen from behind and tilted so tools
	// point up and away (the Java first-person view), the bare arm reaching in from the lower right
	const bool bShield = Item && Item->ItemName == TEXT("shield");
	// items are seen from behind (mirrored, Java style): head up-left, handle towards the lower-right corner
	if (Item) Item->SetRelativeRotation(bBlock ? FRotator(0.f, 45.f, 0.f) : (bShield ? FRotator(0.f, -80.f, 0.f) : FRotator(-8.f, -65.f, 0.f)));
	// the held block sits a little higher and further in than a tool, fully on screen
	if (Item) Item->SetRelativeLocation(bBlock ? FVector(0.f, -4.f, 3.f) : FVector::ZeroVector);
	if (Arm) Arm->SetRelativeRotation(FRotator(16.f, -14.f, 0.f));

	if (Arm) Arm->SetVisibility(bShownEmpty && IsVisible(), true);   // the owner hides the whole hand in third person
	(void)Alpha;
}
