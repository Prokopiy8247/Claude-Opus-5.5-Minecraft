// Living entities: health, armour, effects, equipment, Minecraft movement physics, death & drops.
#pragma once

#include "CoreMinimal.h"
#include "Game/MCEntity.h"
#include "MCLiving.generated.h"

UCLASS(Abstract)
class UNREAL_MINECRAFT_API AMCLiving : public AMCEntity
{
	GENERATED_BODY()
public:
	AMCLiving();

	float Health = 20.f;
	float MaxHealth = 20.f;
	float Absorption = 0.f;
	int32 HurtTime = 0;              // red flash ticks
	int32 DeathTime = 0;             // > 0 while dying
	int32 InvulnerableTime = 0;      // damage immunity frames (10 ticks after a hit)
	float LastHurtAmount = 0.f;
	int32 AirSupply = 300;
	int32 MaxAir = 300;
	float MoveSpeed = 0.1f;          // attribute movement_speed (blocks/tick scale)
	float FlyingSpeed = 0.02f;
	float KnockbackResistance = 0.f;
	float BaseArmor = 0.f;
	float JumpPower = 0.42f;
	bool bJumping = false;
	bool bSprinting = false;

	bool bCanBreatheUnderwater = false;
	bool bUndead = false;
	bool bArthropod = false;
	bool bElytraFlying = false;
	int32 NoJumpDelay = 0;
	float MoveForward = 0.f;         // -1..1 input
	float MoveStrafe = 0.f;          // -1..1 input (positive = left, Minecraft convention)
	float MoveUp = 0.f;
	float HeadYaw = 0.f;             // independent head rotation
	float BodyYaw = 0.f;
	float PrevBodyYaw = 0.f;
	float LimbSwing = 0.f;           // walk animation phase
	float LimbSwingAmount = 0.f;
	float PrevLimbSwingAmount = 0.f;
	float AttackAnim = 0.f;          // arm swing 0..1
	int32 SwingTicks = 0;
	bool bSwinging = false;
	TWeakObjectPtr<AMCEntity> LastAttacker;
	int32 LastAttackerTime = 0;
	TWeakObjectPtr<AMCEntity> LastHurtMob;
	FString LastDeathMessage;

	TArray<FMCEffectInstance> Effects;
	FMCItemStack Equipment[(int32)EMCEquipSlot::Count];
	float DropChances[(int32)EMCEquipSlot::Count] = { 0.085f, 0.085f, 0.085f, 0.085f, 0.085f, 0.085f };

	virtual bool IsLiving() const override { return true; }
	virtual bool IsAlive() const override { return !bRemoved && Health > 0.f && DeathTime == 0; }
	virtual bool IsPickable() const override { return IsAlive(); }
	virtual bool IsPushable() const override { return IsAlive(); }
	virtual void TickEntity() override;
	virtual bool Hurt(const FMCDamage& Damage, float Amount) override;
	virtual void Serialize(FArchive& Ar) override;

	/** Movement input applied every tick (Minecraft LivingEntity.travel). */
	virtual void Travel(float Strafe, float Up, float Forward);
	virtual void Jump();
	virtual void AIStep();
	virtual void Die(const FMCDamage& Damage);
	virtual void DropLoot(const FMCDamage& Damage, bool bPlayerKill) {}
	virtual int32 GetXPReward() const { return 0; }
	virtual float GetFrictionMultiplier() const { return 1.f; }
	virtual bool CanBreatheUnderwater() const { return bCanBreatheUnderwater || HasEffect(EMCEffect::WaterBreathing); }
	virtual void OnHurtEffect(const FMCDamage& Damage) {}
	virtual void OnDeathAnimationFinished();
	virtual bool IsSensitiveToWater() const { return false; }
	virtual bool IsAffectedByPotions() const { return true; }
	virtual float GetSpeed() const;
	virtual bool OnClimbable() const;
	/** Sneaking players stop at block edges. */
	virtual bool PreventsEdgeFall() const { return false; }

	void Heal(float Amount);
	void SetHealth(float H) { Health = FMath::Clamp(H, 0.f, GetMaxHealth()); }
	float GetMaxHealth() const;
	void Knockback(double Strength, double DirX, double DirY);
	void Swing();
	int32 GetArmorValue() const;
	float GetArmorToughness() const;
	int32 GetProtectionLevel(const FMCDamage& Damage) const;
	float ApplyArmor(const FMCDamage& Damage, float Amount);
	float ApplyMagicReduction(const FMCDamage& Damage, float Amount);
	void DamageArmor(float Amount);

	// effects
	bool HasEffect(EMCEffect E) const;
	const FMCEffectInstance* GetEffect(EMCEffect E) const;
	int32 EffectAmp(EMCEffect E) const { const FMCEffectInstance* I = GetEffect(E); return I ? I->Amplifier : -1; }
	bool AddEffect(const FMCEffectInstance& E);
	void RemoveEffect(EMCEffect E);
	void ClearEffects();
	void TickEffects();
	void ApplyInstantEffect(EMCEffect E, int32 Amplifier, AMCEntity* Source);

	/** Equipment access (players map these onto their inventory). */
	virtual FMCItemStack& GetItem(EMCEquipSlot S) { return Equipment[(int32)S]; }
	virtual const FMCItemStack& GetItem(EMCEquipSlot S) const { return Equipment[(int32)S]; }
	FMCItemStack& MainHand() { return GetItem(EMCEquipSlot::MainHand); }
	const FMCItemStack& MainHandConst() const { return GetItem(EMCEquipSlot::MainHand); }
	FMCItemStack& OffHand() { return GetItem(EMCEquipSlot::OffHand); }
	void SetItem(EMCEquipSlot S, const FMCItemStack& Stack) { GetItem(S) = Stack; }
	void BreakItem(EMCEquipSlot S);
	/** Enchantment level from all equipped items relevant for E. */
	int32 GetEnchantTotal(EMCEnchant E) const;
	int32 GetEnchantMax(EMCEnchant E) const;
	bool CanSee(const AMCEntity* Other) const;
	void LookAt(const FVector& Target, float MaxYawStep = 30.f, float MaxPitchStep = 30.f);

protected:
	void TickAir();
	void TickDeath();
	void UpdateWalkAnimation();
	virtual void OnFellOnGround(float Distance, FMCState Landed) override;
	virtual void CheckFallDamage(double DeltaZ, bool bGroundNow) override;
	void TickFreezing();
};
