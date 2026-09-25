// Non-living entities: dropped items, XP orbs, falling blocks, primed TNT, projectiles, vehicles,
// end crystals, lightning, item frames, eyes of ender, fireworks, lingering clouds.
#pragma once

#include "CoreMinimal.h"
#include "Game/MCEntity.h"
#include "World/MCBlockEntity.h"
#include "MCEntities.generated.h"

class UStaticMeshComponent;
class UMCItemVisualComponent;
class UMCRigComponent;
class UPointLightComponent;

UCLASS()
class UNREAL_MINECRAFT_API AMCItemEntity : public AMCEntity
{
	GENERATED_BODY()
public:
	AMCItemEntity();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UMCItemVisualComponent> ItemVisual;
	FMCItemStack Stack;
	int32 PickupDelay = 10;
	int32 Lifetime = 6000;
	float Bob = 0.f;
	TWeakObjectPtr<AMCEntity> Thrower;
	virtual void InitEntity() override;
	virtual void TickEntity() override;
	virtual void UpdateVisual(float Alpha, float DeltaSeconds) override;
	virtual void OnPlayerTouch(AMCPlayer* Player) override;
	virtual bool Hurt(const FMCDamage& Damage, float Amount) override;
	virtual void Serialize(FArchive& Ar) override;
	void SetStack(const FMCItemStack& S);
	void TryMerge();
};

UCLASS()
class UNREAL_MINECRAFT_API AMCXPOrb : public AMCEntity
{
	GENERATED_BODY()
public:
	AMCXPOrb();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	int32 Value = 1;
	int32 Lifetime = 6000;
	virtual void TickEntity() override;
	virtual void UpdateVisual(float Alpha, float DeltaSeconds) override;
	virtual void OnPlayerTouch(AMCPlayer* Player) override;
	virtual void Serialize(FArchive& Ar) override;
	static int32 SplitValue(int32 Total);
};

UCLASS()
class UNREAL_MINECRAFT_API AMCFallingBlock : public AMCEntity
{
	GENERATED_BODY()
public:
	AMCFallingBlock();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UMCItemVisualComponent> BlockVisual;
	FMCState State = 0;
	int32 FallTime = 0;
	bool bHurtEntities = false;
	virtual void InitEntity() override;
	virtual void TickEntity() override;
	virtual void Serialize(FArchive& Ar) override;
};

UCLASS()
class UNREAL_MINECRAFT_API AMCPrimedTNT : public AMCEntity
{
	GENERATED_BODY()
public:
	AMCPrimedTNT();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UMCItemVisualComponent> BlockVisual;
	int32 Fuse = 80;
	float Power = 4.f;
	TWeakObjectPtr<AMCEntity> Igniter;
	virtual void InitEntity() override;
	virtual void TickEntity() override;
	virtual void UpdateVisual(float Alpha, float DeltaSeconds) override;
	virtual void Serialize(FArchive& Ar) override;
};

UENUM()
enum class EMCProjectile : uint8
{
	Arrow, SpectralArrow, Trident, Snowball, Egg, EnderPearl, ExpBottle, Potion, LingeringPotion, Fireball, SmallFireball,
	DragonFireball, WitherSkull, ShulkerBullet, LlamaSpit, WindCharge, BreezeWindCharge, FishingBobber, Spear
};

UCLASS()
class UNREAL_MINECRAFT_API AMCProjectile : public AMCEntity
{
	GENERATED_BODY()
public:
	AMCProjectile();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UMCItemVisualComponent> Visual;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
	EMCProjectile Type = EMCProjectile::Arrow;
	TWeakObjectPtr<AMCEntity> Shooter;
	TWeakObjectPtr<AMCEntity> HomingTarget;
	float Damage = 2.f;
	bool bCritical = false;
	bool bInGround = false;
	bool bPickup = true;          // arrows fired by players in survival
	bool bCreativePickup = false;
	int32 Knockback = 0;
	int32 Pierce = 0;
	int32 LifeInGround = 0;
	int32 Shake = 0;
	bool bFlame = false;
	bool bDangerous = false;      // blue wither skull
	int32 Loyalty = 0;
	bool bReturning = false;
	FMCItemStack Item;            // trident / potion / thrown item
	FVector Accel = FVector::ZeroVector; // fireballs
	TSet<AMCEntity*> PiercedIds;
	virtual void InitEntity() override;
	virtual void TickEntity() override;
	virtual void UpdateVisual(float Alpha, float DeltaSeconds) override;
	virtual void OnPlayerTouch(AMCPlayer* Player) override;
	virtual bool Hurt(const FMCDamage& Damage, float Amount) override;
	virtual bool IsPickable() const override;
	virtual void Serialize(FArchive& Ar) override;
	void Shoot(const FVector& Dir, float Speed, float Inaccuracy);
	void OnHitBlock(const struct FMCRayHit& Hit);
	void OnHitEntity(AMCEntity* E);
	bool ShouldHit(AMCEntity* E) const;
	float GetGravity() const;
	float GetDrag() const;
};

UCLASS()
class UNREAL_MINECRAFT_API AMCBoat : public AMCEntity
{
	GENERATED_BODY()
public:
	AMCBoat();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	FName Wood = TEXT("oak");
	bool bChest = false;
	float Damage = 0.f;
	int32 HurtTicks = 0;
	float PaddleL = 0.f, PaddleR = 0.f;
	float DeltaYaw = 0.f;
	FMCContainer Chest;
	virtual void InitEntity() override;
	virtual void TickEntity() override;
	virtual void UpdateVisual(float Alpha, float DeltaSeconds) override;
	virtual bool Hurt(const FMCDamage& D, float Amount) override;
	virtual bool Interact(AMCPlayer* Player, bool bOffHand) override;
	virtual bool IsPickable() const override { return true; }
	virtual bool CanCollideWith(const AMCEntity* Other) const override { return true; }
	virtual FVector GetPassengerOffset(const AMCEntity* Passenger) const override;
	virtual void Serialize(FArchive& Ar) override;
	FName ItemName() const;
};

UCLASS()
class UNREAL_MINECRAFT_API AMCMinecart : public AMCEntity
{
	GENERATED_BODY()
public:
	AMCMinecart();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UMCItemVisualComponent> Content;
	FName Variant = TEXT("minecart"); // minecart, chest_minecart, furnace_minecart, tnt_minecart, hopper_minecart
	float Damage = 0.f;
	int32 HurtTicks = 0;
	FMCContainer Chest;
	int32 Fuel = 0;                   // furnace minecart
	FVector Push = FVector::ZeroVector;
	bool bOnRail = false;
	int32 TNTFuse = -1;
	virtual void InitEntity() override;
	virtual void TickEntity() override;
	virtual void UpdateVisual(float Alpha, float DeltaSeconds) override;
	virtual bool Hurt(const FMCDamage& D, float Amount) override;
	virtual bool Interact(AMCPlayer* Player, bool bOffHand) override;
	virtual bool IsPickable() const override { return true; }
	virtual bool CanCollideWith(const AMCEntity* Other) const override { return true; }
	virtual FVector GetPassengerOffset(const AMCEntity* Passenger) const override { return FVector(0, 0, 0.35); }
	virtual void Serialize(FArchive& Ar) override;
private:
	void MoveAlongTrack(const FMCBlockPos& RailPos, FMCState Rail);
	void GetRailExits(int32 Shape, FIntVector& A, FIntVector& B) const;
};

UCLASS()
class UNREAL_MINECRAFT_API AMCEndCrystal : public AMCEntity
{
	GENERATED_BODY()
public:
	AMCEndCrystal();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Core;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Cage;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Beam;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Base;
	bool bShowBottom = true;
	bool bSpike = false;
	FVector BeamTarget = FVector::ZeroVector;
	bool bHasBeam = false;
	float Spin = 0.f;
	virtual void TickEntity() override;
	virtual void UpdateVisual(float Alpha, float DeltaSeconds) override;
	virtual bool Hurt(const FMCDamage& D, float Amount) override;
	virtual bool IsPickable() const override { return true; }
	virtual void Serialize(FArchive& Ar) override;
};

UCLASS()
class UNREAL_MINECRAFT_API AMCLightning : public AMCEntity
{
	GENERATED_BODY()
public:
	AMCLightning();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Flash;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Bolt;
	int32 Life = 2;
	int32 Flashes = 0;
	bool bVisualOnly = false;
	virtual void InitEntity() override;
	virtual void TickEntity() override;
	virtual void UpdateVisual(float Alpha, float DeltaSeconds) override;
	virtual bool ShouldSave() const override { return false; }
};

UCLASS()
class UNREAL_MINECRAFT_API AMCItemFrame : public AMCEntity
{
	GENERATED_BODY()
public:
	AMCItemFrame();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Frame;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UMCItemVisualComponent> ItemVisual;
	FMCItemStack Item;
	EMCFace Facing = EMCFace::North;
	int32 ItemRotation = 0;
	bool bGlow = false;
	virtual void InitEntity() override;
	virtual void TickEntity() override;
	virtual void UpdateVisual(float Alpha, float DeltaSeconds) override;
	virtual bool Hurt(const FMCDamage& D, float Amount) override;
	virtual bool Interact(AMCPlayer* Player, bool bOffHand) override;
	virtual bool IsPickable() const override { return true; }
	virtual void Serialize(FArchive& Ar) override;
};

UCLASS()
class UNREAL_MINECRAFT_API AMCEyeOfEnder : public AMCEntity
{
	GENERATED_BODY()
public:
	AMCEyeOfEnder();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UMCItemVisualComponent> Visual;
	FVector TargetPos = FVector::ZeroVector;
	int32 Life = 0;
	bool bSurvive = true;
	virtual void TickEntity() override;
	virtual bool ShouldSave() const override { return false; }
};

UCLASS()
class UNREAL_MINECRAFT_API AMCFirework : public AMCEntity
{
	GENERATED_BODY()
public:
	AMCFirework();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UMCItemVisualComponent> Visual;
	int32 Life = 0, LifeTime = 30;
	TWeakObjectPtr<AMCEntity> AttachedTo; // elytra boost
	FMCItemStack Item;
	virtual void TickEntity() override;
	virtual bool ShouldSave() const override { return false; }
	void Explode();
};

UCLASS()
class UNREAL_MINECRAFT_API AMCAreaCloud : public AMCEntity
{
	GENERATED_BODY()
public:
	AMCAreaCloud();
	float Radius = 3.f;
	int32 Duration = 600;
	EMCEffect Effect = EMCEffect::None;
	int32 EffectDuration = 200;
	uint8 Amplifier = 0;
	FColor Color = FColor::Purple;
	bool bDragonBreath = false;
	TWeakObjectPtr<AMCEntity> Owner;
	virtual void TickEntity() override;
	virtual void Serialize(FArchive& Ar) override;
};
