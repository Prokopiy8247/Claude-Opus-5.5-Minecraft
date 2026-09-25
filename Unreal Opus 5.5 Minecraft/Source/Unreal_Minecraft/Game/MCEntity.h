// Base class of every simulated entity (players, mobs, items, projectiles, vehicles...).
// Simulation runs at a fixed 20 Hz in block units; the actor transform is interpolated every frame.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/MCCore.h"
#include "Items/MCItems.h"
#include "MCEntity.generated.h"

class FMCWorld;
class AMCGame;
class AMCPlayer;
class AMCEntity;

UENUM()
enum class EMCEntityKind : uint8
{
	Other, Item, XPOrb, FallingBlock, TNT, Projectile, Boat, Minecart, EndCrystal, Lightning, ItemFrame, EyeOfEnder,
	Firework, AreaCloud, Player, Mob
};

/** Damage description (Minecraft damage types). */
struct UNREAL_MINECRAFT_API FMCDamage
{
	FName Type = TEXT("generic");
	TWeakObjectPtr<AMCEntity> Attacker; // responsible entity (shooter / owner)
	TWeakObjectPtr<AMCEntity> Direct;   // direct entity (arrow, fireball)
	FVector SourcePos = FVector::ZeroVector;
	bool bHasSourcePos = false;
	bool bBypassArmor = false;
	bool bBypassInvulnerability = false; // /kill, void
	bool bBypassCreative = false;
	bool bFire = false;
	bool bExplosion = false;
	bool bProjectile = false;
	bool bMagic = false;
	bool bFall = false;
	bool bNoKnockback = false;
	bool bScalesWithDifficulty = false;
	bool bCritical = false;
	float KnockbackStrength = 0.4f;

	static FMCDamage Of(FName InType);
	static FMCDamage Mob(AMCEntity* Attacker);
	static FMCDamage PlayerAttack(AMCPlayer* P);
	static FMCDamage Projectile(AMCEntity* Projectile, AMCEntity* Shooter, FName InType = TEXT("arrow"));
	static FMCDamage Explosion(const FVector& Pos, AMCEntity* Source);
	FString DeathMessage(const FString& Victim) const;
};

UCLASS(Abstract)
class UNREAL_MINECRAFT_API AMCEntity : public AActor
{
	GENERATED_BODY()
public:
	AMCEntity();

	FMCWorld* World = nullptr;
	UPROPERTY(Transient) TObjectPtr<AMCGame> Game = nullptr;
	EMCEntityKind Kind = EMCEntityKind::Other;
	int64 EntityId = 0;
	FName TypeId;

	// ---- simulation state (block units, Z up)
	FVector Pos = FVector::ZeroVector;      // bottom centre of the bounding box
	FVector PrevPos = FVector::ZeroVector;  // previous tick (interpolation)
	FVector Vel = FVector::ZeroVector;      // blocks per tick
	float Yaw = 0.f, Pitch = 0.f;           // degrees, UE convention (yaw 0 = +X)
	float PrevYaw = 0.f, PrevPitch = 0.f;
	float Width = 0.6f, Height = 1.8f, EyeHeight = 1.62f;
	float StepHeight = 0.6f;
	double Gravity = 0.08;
	double AirDrag = 0.98;
	bool bOnGround = false;
	bool bHorizontalCollision = false;
	bool bVerticalCollision = false;
	bool bCollidedBelow = false;
	bool bInWater = false, bInLava = false, bEyesInWater = false, bInPowderSnow = false;
	bool bWasInWater = false;
	bool bNoPhysics = false;       // no collision
	bool bNoGravity = false;
	bool bPersistent = false;      // never despawns
	bool bRemoved = false;
	bool bInvulnerable = false;
	bool bFireImmune = false;
	bool bSilent = false;
	bool bGlowing = false;
	bool bSneaking = false;       // crouching (players, cats) - read by blocks (magma, sculk, powder snow)
	float FallDistance = 0.f;
	int32 FireTicks = 0;           // > 0 = burning
	int32 Age = 0;                 // ticks alive
	int32 PortalTime = 0;          // ticks spent in a nether portal
	int32 PortalCooldown = 0;
	bool bInsidePortal = false;
	FVector StuckSpeedMultiplier = FVector::OneVector; // cobweb / berry bush / powder snow
	float BlockSpeedFactor = 1.f;
	int32 FreezeTicks = 0;
	FString CustomName;

	TWeakObjectPtr<AMCEntity> Vehicle;
	TArray<TWeakObjectPtr<AMCEntity>> Passengers;

	// ---- lifecycle
	/** Called once after spawn and registration in a world. */
	virtual void InitEntity() {}
	/** Fixed 20 Hz simulation step. */
	virtual void TickEntity();
	/** Per-frame visual update (Alpha = interpolation factor between PrevPos and Pos). */
	virtual void UpdateVisual(float Alpha, float DeltaSeconds);
	/** Remove from the world (destroys the actor at the end of the tick). */
	virtual void Discard();
	virtual bool IsAlive() const { return !bRemoved; }
	virtual bool IsLiving() const { return false; }
	virtual bool IsPickable() const { return false; }       // targetable by the crosshair
	virtual bool IsPushable() const { return false; }
	virtual bool CanCollideWith(const AMCEntity* Other) const { return false; } // solid for others (boats, shulkers)
	virtual bool ShouldSave() const { return !bRemoved; }
	/** Damage entry point. Returns true if damage was applied. */
	virtual bool Hurt(const FMCDamage& Damage, float Amount) { return false; }
	/** Player right-clicked this entity. */
	virtual bool Interact(AMCPlayer* Player, bool bOffHand) { return false; }
	/** Player touched this entity (item pickup, xp). */
	virtual void OnPlayerTouch(AMCPlayer* Player) {}
	/** Lightning bolt struck nearby. */
	virtual void OnLightning() { SetOnFire(8); }
	virtual FString GetDisplayName() const;
	virtual FVector GetEyePos() const { return Pos + FVector(0, 0, EyeHeight); }
	FVector GetLookDir() const;
	virtual void Serialize(FArchive& Ar);
	virtual void OnDimensionChanged() {}

	// ---- helpers
	FMCBox GetBox() const;
	FMCBox GetBoxAt(const FVector& P) const;
	void SetPosition(const FVector& P, bool bResetInterpolation = true);
	void SetRotation(float InYaw, float InPitch) { Yaw = InYaw; Pitch = InPitch; }
	/** Collision-resolved movement (Minecraft style, including step-up). */
	void Move(const FVector& Delta, bool bPreventEdgeFall = false);
	void ApplyGravityAndDrag();
	void UpdateFluidState();
	void UpdateInsideBlocks();
	void SetOnFire(int32 Seconds);
	void Extinguish() { FireTicks = 0; }
	bool IsOnFire() const { return FireTicks > 0 && !bFireImmune; }
	void AddVelocity(const FVector& V) { Vel += V; }
	void PushAwayFrom(AMCEntity* Other);
	float DistanceTo(const AMCEntity* Other) const;
	double DistanceSqTo(const FVector& P) const { return (Pos - P).SizeSquared(); }
	void StartRiding(AMCEntity* V);
	void StopRiding();
	bool IsPassenger() const { return Vehicle.IsValid(); }
	virtual FVector GetPassengerOffset(const AMCEntity* Passenger) const { return FVector(0, 0, Height); }
	FMCBlockPos BlockPos() const { return FMCBlockPos(MC::FloorToInt(Pos.X), MC::FloorToInt(Pos.Y), MC::FloorToInt(Pos.Z)); }
	FMCBlockPos BlockBelow() const { return FMCBlockPos(MC::FloorToInt(Pos.X), MC::FloorToInt(Pos.Y), MC::FloorToInt(Pos.Z - 0.2)); }
	float GetBrightness() const;
	/** Material lighting terms for the entity's model from the voxel light at its centre (see UMCRigComponent::SetLighting). */
	void GetModelLight(float& OutFill, float& OutTorch) const;
	/** Teleport (possibly between dimensions). */
	void TeleportTo(const FVector& P);
	FMCRandom& Rand() const;
	void PlaySound(FName Sound, float Volume = 1.f, float PitchMul = 1.f) const;
	FVector InterpolatedPos(float Alpha) const { return FMath::Lerp(PrevPos, Pos, (double)Alpha); }
	FVector ToWorldUU(const FVector& BlockPos) const { return BlockPos * MC::BlockSize; }

	/** Root for visual components (rotates with yaw). */
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> VisualRoot;

protected:
	virtual void HandleFireDamage();
	virtual void OnFellOnGround(float Distance, FMCState Landed) {}
	virtual void CheckFallDamage(double DeltaZ, bool bGroundNow);
	int32 LastFireDamageTick = 0;
};
