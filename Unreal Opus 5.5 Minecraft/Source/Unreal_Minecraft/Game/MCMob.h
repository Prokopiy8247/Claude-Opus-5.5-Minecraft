// Mobs: data-driven definitions, AI archetypes, pathfinding, breeding, taming, trading, bosses.
#pragma once

#include "CoreMinimal.h"
#include "Game/MCLiving.h"
#include "Game/MCMenu.h"
#include "MCMob.generated.h"

class UMCRigComponent;
class UMCItemVisualComponent;

enum class EMCMobCategory : uint8 { Monster, Creature, Ambient, WaterCreature, WaterAmbient, Underground, Misc, Boss };

enum class EMCMobAI : uint8
{
	Animal,        // wander, panic, breed, tempt, follow parent
	Zombie,        // melee chaser (zombie, husk, drowned, zombified piglin (neutral), piglin brute...)
	Skeleton,      // bow strafing (skeleton, stray, bogged, parched, pillager crossbow)
	Creeper,       // swell & explode
	Spider,        // pounce, wall climbing, neutral in daylight
	Enderman,      // teleport, stare aggro, block carrying
	Slime,         // hop, split
	Ghast,         // flying fireball shooter
	Blaze,         // hovering triple fireballs
	Flyer,         // bat, parrot, allay, bee, vex, phantom, happy ghast
	Swimmer,       // fish, squid, dolphin, guardian, axolotl, tadpole, nautilus
	Villager,      // wander, work, trade, flee zombies, sleep
	Golem,         // iron / snow / copper golem
	Tameable,      // wolf, cat, parrot
	Mount,         // horse, donkey, mule, camel, llama, strider
	Illager,       // vindicator, evoker, witch, ravager
	Shulker,       // stationary bullet shooter
	Warden,        // blind, sniffs, sonic boom
	Silverfish,    // swarm
	Dragon,        // Ender Dragon boss
	Wither,        // Wither boss
	Static         // armor stands, misc
};

struct FMCMobDef
{
	FName Id;
	FString Name;
	EMCMobCategory Category = EMCMobCategory::Creature;
	EMCMobAI AI = EMCMobAI::Animal;
	float Width = 0.9f, Height = 0.9f, EyeHeight = 0.8f;
	float MaxHealth = 10.f;
	float Speed = 0.25f;           // movement_speed attribute
	float AttackDamage = 2.f;
	float FollowRange = 16.f;
	float Armor = 0.f;
	float KnockbackResist = 0.f;
	int32 XP = 1;
	bool bHostile = false;         // attacks players on sight
	bool bNeutral = false;         // attacks when provoked
	bool bUndead = false, bArthropod = false, bFireImmune = false, bFlying = false, bSwimmer = false, bAmphibious = false;
	bool bBurnsInDaylight = false, bBreathesUnderwater = false, bCanBreed = false, bHasBaby = true, bNoGravity = false;
	bool bBoss = false;
	FName Rig;                     // visual rig id
	FColor Tint = FColor::White;   // fallback / variant tint
	int32 NumVariants = 1;
	FName LootTable;
	TArray<FName> BreedItems;      // item names or #tags
	FName Sound;                   // sound prefix (e.g. "zombie" -> zombie_ambient/hurt/death)
	float Scale = 1.f;
	float StepHeight = 0.6f;
	float AmbientSoundChance = 0.012f;
};

namespace MCMobs
{
	UNREAL_MINECRAFT_API void Init();
	UNREAL_MINECRAFT_API const FMCMobDef* Find(FName Id);
	UNREAL_MINECRAFT_API const TArray<FMCMobDef>& All();
}

/** Grid A* for walking mobs. */
struct UNREAL_MINECRAFT_API FMCPathfinder
{
	static bool FindPath(const FMCWorld& W, const FMCBlockPos& Start, const FMCBlockPos& Goal, int32 MaxNodes, float MobWidth, float MobHeight,
		bool bCanSwim, bool bAvoidWater, TArray<FMCBlockPos>& OutPath, int32 MaxDrop = 3);
	static bool IsWalkable(const FMCWorld& W, const FMCBlockPos& P, int32 HeightBlocks, bool bCanSwim);
};

UCLASS()
class UNREAL_MINECRAFT_API AMCMob : public AMCLiving
{
	GENERATED_BODY()
public:
	AMCMob();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UMCRigComponent> Rig;
	UPROPERTY(Transient) TObjectPtr<UMCItemVisualComponent> HeldVisual;

	const FMCMobDef* Def = nullptr;
	int32 Variant = 0;
	int32 GrowAge = 0;             // < 0 baby, counts up to 0; > 0 breeding cooldown
	int32 InLove = 0;
	TWeakObjectPtr<AMCPlayer> LoveCause;
	TWeakObjectPtr<AMCLiving> Target;
	TWeakObjectPtr<AMCEntity> Owner;   // tamed
	bool bTamed = false;
	bool bSitting = false;
	bool bSaddled = false;
	bool bSheared = false;
	bool bCharged = false;          // creeper
	bool bAngry = false;
	int32 AngerTime = 0;
	int32 AttackCooldown = 0;
	int32 SwellTime = 0;            // creeper
	int32 PrevSwellTime = 0;
	int32 SwellDir = -1;
	int32 GoalTimer = 0;
	int32 AmbientSoundTime = 0;
	int32 NoActionTime = 0;
	int32 PanicTime = 0;
	int32 EatAnim = 0;
	int32 LayEggTime = 6000;
	int32 ShootTime = 0;
	int32 ChargeTime = 0;
	int32 TeleportCooldown = 0;
	int32 SplitSize = 1;            // slime / magma cube size
	FMCBlockPos HomePos;
	bool bHasHome = false;
	FVector WanderTarget = FVector::ZeroVector;
	bool bHasWander = false;
	FVector FlyTarget = FVector::ZeroVector;
	TArray<FMCBlockPos> Path;
	int32 PathIndex = 0;
	int32 RepathTimer = 0;
	FMCBlockPos PathGoal;
	FMCState CarriedBlock = 0;      // enderman
	FMCItemStack HeldDisplay;
	TArray<FMCTrade> Trades;        // villagers / wandering traders
	FName Profession;
	int32 VillagerLevel = 1;
	int32 VillagerXP = 0;
	TWeakObjectPtr<AMCPlayer> TradingPlayer;
	bool bNaturalSpawn = false;
	bool bNoAI = false;
	uint8 OffscreenFrames = 0;       // visual update throttle while off screen             // NBT NoAI: no goals, no movement (summon {NoAI:1b})
	int32 DespawnCounter = 0;
	float AnimTime = 0.f;
	FColor Color = FColor::White;   // sheep wool colour / collar
	uint8 DyeColor = 0;
	float SpeedModifier = 1.f;      // navigation speed multiplier (panic, charge, tempt)
	int32 SpecialTimer = 0;         // archetype specific timer (laser charge, sonic boom, spit, roar...)
	int32 SubState = 0;             // archetype specific state machine
	int32 Anger = 0;                // warden anger / piglin admiring
	int32 TameAttempts = 0;
	int32 Temper = 0;               // horses
	bool bHasChest = false;         // donkeys, mules, llamas
	bool bHarnessed = false;        // happy ghast
	bool bTrusting = false;         // ocelot
	FMCContainer MobInventory;      // chested mounts, allays, piglins
	TWeakObjectPtr<AMCEntity> LeashHolder;
	TWeakObjectPtr<AMCEntity> FollowTarget; // parent / owner / tempting player / caravan leader
	FMCItemStack WantedItem;        // allay
	FVector AimPos = FVector::ZeroVector;
	int32 LastPathTick = 0;

	virtual float GetSpeed() const override;
	virtual bool IsAffectedByPotions() const override;
	virtual bool CanBreatheUnderwater() const override;

	void SetDefinition(const FMCMobDef* InDef, int32 InVariant);
	virtual void InitEntity() override;
	virtual void TickEntity() override;
	virtual void AIStep() override;
	virtual void UpdateVisual(float Alpha, float DeltaSeconds) override;
	virtual bool Hurt(const FMCDamage& Damage, float Amount) override;
	virtual void Die(const FMCDamage& Damage) override;
	virtual void DropLoot(const FMCDamage& Damage, bool bPlayerKill) override;
	virtual int32 GetXPReward() const override;
	virtual bool Interact(AMCPlayer* Player, bool bOffHand) override;
	virtual FString GetDisplayName() const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual bool IsSensitiveToWater() const override;
	virtual void OnLightning() override;
	virtual bool CanCollideWith(const AMCEntity* Other) const override;
	virtual void OnDeathAnimationFinished() override;
	virtual FVector GetPassengerOffset(const AMCEntity* Passenger) const override;

	bool IsBaby() const { return GrowAge < 0; }
	bool IsHostile() const { return Def && (Def->bHostile || bAngry); }
	bool IsBreedItem(const FMCItemStack& S) const;
	bool IsTemptedBy(const AMCPlayer* P) const;
	void SetBaby(bool bBaby) { GrowAge = bBaby ? -24000 : 0; }
	void SetAngryAt(AMCLiving* Who, int32 Ticks = 400);
	/** Try to spawn a baby with Partner. */
	void Breed(AMCMob* Partner);
	void Explode();
	void GenerateTrades();
	bool ShouldDespawn(double DistToPlayerSq) const;
	virtual void ApplyVariantVisuals();

	// movement helpers
	void MoveTowards(const FVector& Goal, float SpeedMul);
	bool NavigateTo(const FVector& Goal, float SpeedMul);
	void StopNavigation() { Path.Reset(); PathIndex = 0; MoveForward = 0.f; }
	bool HasPath() const { return PathIndex < Path.Num(); }
	void FaceTowards(const FVector& P, float MaxTurn = 20.f);
	bool FindRandomWanderTarget(int32 Radius, int32 VRadius, FVector& Out, bool bPreferWater = false) const;
	AMCPlayer* FindNearestPlayer(float Range, bool bNeedSight) const;
	AMCLiving* FindNearestMob(float Range, TFunctionRef<bool(AMCLiving*)> Pred) const;
	void ShootProjectile(FName Type, AMCLiving* At, float Speed, float Inaccuracy);
	bool DoMeleeAttack(AMCLiving* Victim);
	bool IsSunBurnTick() const;

protected:
	// AI archetypes
	void TickAnimalAI();
	void TickZombieAI();
	void TickSkeletonAI();
	void TickCreeperAI();
	void TickSpiderAI();
	void TickEndermanAI();
	void TickSlimeAI();
	void TickGhastAI();
	void TickBlazeAI();
	void TickFlyerAI();
	void TickSwimmerAI();
	void TickVillagerAI();
	void TickGolemAI();
	void TickTameableAI();
	void TickIllagerAI();
	void TickShulkerAI();
	void TickWardenAI();
	void TickCommonTargeting();
	void TickWander(float SpeedMul, int32 Chance = 120);
	void TickFollowPath(float SpeedMul);
	void TickFlyMovement(float Speed);
	void TickSwimMovement(float Speed);
	void TickSpecialAbilities();
	bool TeleportRandomly(float Range);
	bool TeleportTowards(const FVector& Dest);
	void UpdateAmbientSound();
	float AttackReach(const AMCLiving* Victim) const;
};

/** Ender Dragon: phases, crystals, breath, perching on the exit portal, death sequence. */
UCLASS()
class UNREAL_MINECRAFT_API AMCEnderDragon : public AMCMob
{
	GENERATED_BODY()
public:
	AMCEnderDragon();
	enum class EPhase : uint8 { Circling, Strafing, Approaching, Landing, Perching, Breathing, TakingOff, Charging, Dying };
	EPhase Phase = EPhase::Circling;
	int32 PhaseTime = 0;
	FVector Waypoint = FVector::ZeroVector;
	int32 CircleIndex = 0;
	bool bClockwise = true;
	TWeakObjectPtr<AMCEntity> HealingCrystal;
	float WingFlap = 0.f, PrevWingFlap = 0.f;
	int32 DeathTicks = 0;
	float DamageSinceStateChange = 0.f;
	TArray<FVector> PosHistory;     // neck/tail segment trail

	virtual void InitEntity() override;
	virtual void AIStep() override;
	virtual void TickEntity() override;
	virtual bool Hurt(const FMCDamage& Damage, float Amount) override;
	virtual void Die(const FMCDamage& Damage) override;
	virtual void UpdateVisual(float Alpha, float DeltaSeconds) override;
	virtual bool IsPushable() const override { return false; }
	/** Multi-part hit test (head, neck, body, wings, tail) against a ray. */
	bool RayHitParts(const FVector& Origin, const FVector& Dir, double MaxDist, double& OutT) const;
	void GetPartBoxes(TArray<FMCBox>& Out) const;
	void SetPhase(EPhase P);
private:
	void TickCrystalHealing();
	void DestroyBlocksInBody();
	void TickDeathSequence();
	FVector PortalCenter() const;
};

/** The Wither: charged spawn explosion, three heads shooting skulls, armour below half health. */
UCLASS()
class UNREAL_MINECRAFT_API AMCWither : public AMCMob
{
	GENERATED_BODY()
public:
	AMCWither();
	int32 InvulTicks = 220;
	int32 HeadTargets[2] = { 0, 0 };
	TWeakObjectPtr<AMCLiving> HeadTarget[3];
	int32 HeadCooldown[3] = { 0, 0, 0 };
	int32 BlockBreakCooldown = 0;
	float HeadYaws[3] = { 0, 0, 0 };
	float HeadPitches[3] = { 0, 0, 0 };

	virtual void InitEntity() override;
	virtual void AIStep() override;
	virtual bool Hurt(const FMCDamage& Damage, float Amount) override;
	virtual void Die(const FMCDamage& Damage) override;
	virtual void UpdateVisual(float Alpha, float DeltaSeconds) override;
	bool IsPowered() const { return Health <= GetMaxHealth() * 0.5f; }
	FVector HeadPos(int32 Index) const;
	/** Checks for a soul sand T + 3 wither skulls after a skull was placed; spawns the Wither. */
	static bool TrySpawnFromStructure(FMCWorld& W, const FMCBlockPos& SkullPos);
};
