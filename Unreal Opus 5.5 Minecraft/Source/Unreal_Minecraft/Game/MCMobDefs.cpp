// Mob definitions (sizes, attributes and behaviour archetypes modelled on Minecraft Java 26.x).
#include "Game/MCMob.h"
#include <initializer_list>

namespace
{
	TArray<FMCMobDef> GMobs;
	TMap<FName, int32> GMobIndex;
	bool GMobsInit = false;

	using C = EMCMobCategory;
	using A = EMCMobAI;

	struct FDefBuilder
	{
		FMCMobDef D;
		FDefBuilder(const TCHAR* Id, C Cat, A AI, float W, float H, float HP, float Speed)
		{
			D.Id = FName(Id);
			FString N = Id;
			N.ReplaceInline(TEXT("_"), TEXT(" "));
			for (int32 i = 0; i < N.Len(); ++i) if (i == 0 || N[i - 1] == TEXT(' ')) N[i] = FChar::ToUpper(N[i]);
			D.Name = N;
			D.Category = Cat; D.AI = AI;
			D.Width = W; D.Height = H; D.EyeHeight = H * 0.85f;
			D.MaxHealth = HP; D.Speed = Speed;
			D.Rig = FName(Id);
			D.Sound = FName(Id);
			D.bHostile = Cat == C::Monster;
			D.bCanBreed = Cat == C::Creature;
			D.XP = Cat == C::Monster ? 5 : (Cat == C::Creature ? 2 : 1);
		}
		FDefBuilder& Dmg(float V) { D.AttackDamage = V; return *this; }
		FDefBuilder& XP(int32 V) { D.XP = V; return *this; }
		FDefBuilder& Armor(float V) { D.Armor = V; return *this; }
		FDefBuilder& KB(float V) { D.KnockbackResist = V; return *this; }
		FDefBuilder& Range(float V) { D.FollowRange = V; return *this; }
		FDefBuilder& Eye(float V) { D.EyeHeight = V; return *this; }
		FDefBuilder& Undead() { D.bUndead = true; return *this; }
		FDefBuilder& Arthropod() { D.bArthropod = true; return *this; }
		FDefBuilder& FireImmune() { D.bFireImmune = true; return *this; }
		FDefBuilder& Flying() { D.bFlying = true; D.bNoGravity = true; return *this; }
		FDefBuilder& Swimmer() { D.bSwimmer = true; D.bBreathesUnderwater = true; return *this; }
		FDefBuilder& Amphibious() { D.bAmphibious = true; D.bBreathesUnderwater = true; return *this; }
		FDefBuilder& Burns() { D.bBurnsInDaylight = true; return *this; }
		FDefBuilder& Neutral() { D.bNeutral = true; D.bHostile = false; return *this; }
		FDefBuilder& Passive() { D.bHostile = false; return *this; }
		FDefBuilder& Hostile() { D.bHostile = true; return *this; }
		FDefBuilder& NoBaby() { D.bHasBaby = false; return *this; }
		FDefBuilder& NoBreed() { D.bCanBreed = false; return *this; }
		FDefBuilder& Breed(std::initializer_list<const TCHAR*> Items) { D.bCanBreed = true; for (const TCHAR* I : Items) D.BreedItems.Add(FName(I)); return *this; }
		FDefBuilder& Rig(const TCHAR* R) { D.Rig = FName(R); return *this; }
		FDefBuilder& Sound(const TCHAR* S) { D.Sound = FName(S); return *this; }
		FDefBuilder& Tint(uint32 Rgb) { D.Tint = FColor((Rgb >> 16) & 255, (Rgb >> 8) & 255, Rgb & 255); return *this; }
		FDefBuilder& Variants(int32 N) { D.NumVariants = N; return *this; }
		FDefBuilder& Scale(float S) { D.Scale = S; return *this; }
		FDefBuilder& Step(float S) { D.StepHeight = S; return *this; }
		FDefBuilder& Boss() { D.bBoss = true; D.Category = C::Boss; D.bHasBaby = false; D.bCanBreed = false; return *this; }
		FDefBuilder& Name(const TCHAR* N) { D.Name = N; return *this; }
		FDefBuilder& NoGravity() { D.bNoGravity = true; return *this; }
		~FDefBuilder()
		{
			if (D.LootTable.IsNone()) D.LootTable = D.Id;
			const int32 I = GMobs.Add(D);
			GMobIndex.Add(D.Id, I);
		}
	};

	void RegisterAll()
	{
		const TCHAR* Seeds[] = { TEXT("wheat_seeds") };
		(void)Seeds;
		// ------------------------------------------------------------------ farm animals
		FDefBuilder(TEXT("pig"), C::Creature, A::Animal, 0.9f, 0.9f, 10, 0.25f).Breed({ TEXT("carrot"), TEXT("potato"), TEXT("beetroot") }).Tint(0xF0A5A2).Variants(3);
		FDefBuilder(TEXT("cow"), C::Creature, A::Animal, 0.9f, 1.4f, 10, 0.2f).Breed({ TEXT("wheat") }).Tint(0x443626).Eye(1.3f).Variants(3);
		FDefBuilder(TEXT("mooshroom"), C::Creature, A::Animal, 0.9f, 1.4f, 10, 0.2f).Breed({ TEXT("wheat") }).Tint(0xA1100F).Eye(1.3f).Variants(2);
		FDefBuilder(TEXT("sheep"), C::Creature, A::Animal, 0.9f, 1.3f, 8, 0.23f).Breed({ TEXT("wheat") }).Tint(0xE7E7E7).Eye(1.2f);
		FDefBuilder(TEXT("chicken"), C::Creature, A::Animal, 0.4f, 0.7f, 4, 0.25f).Breed({ TEXT("wheat_seeds"), TEXT("melon_seeds"), TEXT("pumpkin_seeds"), TEXT("beetroot_seeds"), TEXT("torchflower_seeds"), TEXT("pitcher_pod") }).Tint(0xFFFFFF).Eye(0.6f).Variants(3);
		FDefBuilder(TEXT("goat"), C::Creature, A::Animal, 0.9f, 1.3f, 10, 0.2f).Breed({ TEXT("wheat") }).Dmg(2).Tint(0xD8D2C4);
		FDefBuilder(TEXT("rabbit"), C::Creature, A::Animal, 0.4f, 0.5f, 3, 0.3f).Breed({ TEXT("carrot"), TEXT("golden_carrot"), TEXT("dandelion") }).Tint(0x8B6A4A).Variants(6).Rig(TEXT("rabbit"));
		FDefBuilder(TEXT("turtle"), C::Creature, A::Animal, 1.2f, 0.4f, 30, 0.25f).Breed({ TEXT("seagrass") }).Amphibious().Tint(0x4C8C3C);
		FDefBuilder(TEXT("armadillo"), C::Creature, A::Animal, 0.7f, 0.65f, 12, 0.14f).Breed({ TEXT("spider_eye") }).Tint(0xAD716D);
		FDefBuilder(TEXT("frog"), C::Creature, A::Animal, 0.5f, 0.5f, 10, 0.25f).Breed({ TEXT("slime_ball") }).Amphibious().Tint(0xD07444).Variants(3).Rig(TEXT("frog"));
		FDefBuilder(TEXT("tadpole"), C::WaterAmbient, A::Swimmer, 0.4f, 0.3f, 6, 1.f).Swimmer().NoBreed().NoBaby().Tint(0x6D533D).XP(0);
		FDefBuilder(TEXT("panda"), C::Creature, A::Animal, 1.3f, 1.25f, 20, 0.15f).Breed({ TEXT("bamboo") }).Dmg(6).Tint(0xE7E7E7);
		FDefBuilder(TEXT("fox"), C::Creature, A::Animal, 0.6f, 0.7f, 10, 0.3f).Breed({ TEXT("sweet_berries"), TEXT("glow_berries") }).Dmg(2).Tint(0xE37C21).Variants(2);
		FDefBuilder(TEXT("polar_bear"), C::Creature, A::Animal, 1.4f, 1.4f, 30, 0.25f).Neutral().NoBreed().Dmg(6).Tint(0xF2F2F2);
		FDefBuilder(TEXT("sniffer"), C::Creature, A::Animal, 1.9f, 1.75f, 14, 0.1f).Breed({ TEXT("torchflower_seeds") }).Tint(0x8B3A2A);
		FDefBuilder(TEXT("bee"), C::Creature, A::Flyer, 0.7f, 0.6f, 10, 0.3f).Neutral().Breed({ TEXT("dandelion"), TEXT("poppy"), TEXT("sunflower"), TEXT("cornflower"), TEXT("allium") }).Flying().Dmg(2).Tint(0xEDC343);
		// ------------------------------------------------------------------ pets & mounts
		FDefBuilder(TEXT("wolf"), C::Creature, A::Tameable, 0.6f, 0.85f, 8, 0.3f).Neutral().Breed({ TEXT("beef"), TEXT("cooked_beef"), TEXT("porkchop"), TEXT("cooked_porkchop"), TEXT("chicken"), TEXT("cooked_chicken"), TEXT("mutton"), TEXT("rotten_flesh") }).Dmg(4).Tint(0xD7D3D3).Variants(9);
		FDefBuilder(TEXT("cat"), C::Creature, A::Tameable, 0.6f, 0.7f, 10, 0.3f).Breed({ TEXT("cod"), TEXT("salmon") }).Dmg(3).Tint(0xEFC88E).Variants(11);
		FDefBuilder(TEXT("ocelot"), C::Creature, A::Animal, 0.6f, 0.7f, 10, 0.3f).Breed({ TEXT("cod"), TEXT("salmon") }).Tint(0xEFDE7D);
		FDefBuilder(TEXT("parrot"), C::Creature, A::Flyer, 0.5f, 0.9f, 6, 0.2f).NoBreed().NoBaby().Tint(0x0DA70B).Variants(5);
		FDefBuilder(TEXT("horse"), C::Creature, A::Mount, 1.3964844f, 1.6f, 22, 0.225f).Breed({ TEXT("golden_apple"), TEXT("golden_carrot") }).Tint(0xC09E7D).Variants(7).Step(1.0f);
		FDefBuilder(TEXT("donkey"), C::Creature, A::Mount, 1.3964844f, 1.5f, 22, 0.175f).Breed({ TEXT("golden_apple"), TEXT("golden_carrot") }).Tint(0x534539).Step(1.0f);
		FDefBuilder(TEXT("mule"), C::Creature, A::Mount, 1.3964844f, 1.6f, 22, 0.175f).NoBreed().Tint(0x1B0200).Step(1.0f);
		FDefBuilder(TEXT("skeleton_horse"), C::Creature, A::Mount, 1.3964844f, 1.6f, 15, 0.2f).NoBreed().Undead().Tint(0x68684F).Step(1.0f);
		FDefBuilder(TEXT("zombie_horse"), C::Creature, A::Mount, 1.3964844f, 1.6f, 15, 0.2f).NoBreed().Undead().Tint(0x315234).Step(1.0f);
		FDefBuilder(TEXT("camel"), C::Creature, A::Mount, 1.7f, 2.375f, 32, 0.09f).Breed({ TEXT("cactus") }).Tint(0xFCC369).Step(1.5f);
		FDefBuilder(TEXT("camel_husk"), C::Monster, A::Mount, 1.7f, 2.375f, 32, 0.09f).Undead().NoBreed().NoBaby().Tint(0x8C7A5A).Rig(TEXT("camel")).Name(TEXT("Camel Husk")).Step(1.5f);
		FDefBuilder(TEXT("llama"), C::Creature, A::Mount, 0.9f, 1.87f, 22, 0.175f).Breed({ TEXT("hay_block") }).Tint(0xC09E7D).Variants(4);
		FDefBuilder(TEXT("trader_llama"), C::Creature, A::Mount, 0.9f, 1.87f, 22, 0.175f).Breed({ TEXT("hay_block") }).Tint(0xEAA430).Variants(4);
		FDefBuilder(TEXT("strider"), C::Creature, A::Mount, 0.9f, 1.7f, 20, 0.175f).Breed({ TEXT("warped_fungus") }).FireImmune().Tint(0x9C3436);
		FDefBuilder(TEXT("happy_ghast"), C::Creature, A::Flyer, 4.0f, 4.0f, 20, 0.05f).Breed({ TEXT("snowball") }).Flying().Tint(0xF6F6F6).Scale(1.0f);
		FDefBuilder(TEXT("ghastling"), C::Creature, A::Flyer, 1.0f, 1.0f, 20, 0.05f).NoBreed().NoBaby().Flying().Tint(0xF6F6F6).Rig(TEXT("happy_ghast")).Scale(0.25f);
		FDefBuilder(TEXT("nautilus"), C::WaterCreature, A::Swimmer, 0.875f, 0.95f, 15, 1.f).Swimmer().Breed({ TEXT("pufferfish") }).Tint(0xC9A57A);
		FDefBuilder(TEXT("zombie_nautilus"), C::Monster, A::Swimmer, 0.875f, 0.95f, 15, 1.f).Swimmer().Undead().NoBreed().NoBaby().Tint(0x3D6B5A);
		// ------------------------------------------------------------------ water life
		FDefBuilder(TEXT("cod"), C::WaterAmbient, A::Swimmer, 0.5f, 0.3f, 3, 1.f).Swimmer().NoBreed().NoBaby().Tint(0xC1A76A).XP(1);
		FDefBuilder(TEXT("salmon"), C::WaterAmbient, A::Swimmer, 0.7f, 0.4f, 3, 1.f).Swimmer().NoBreed().NoBaby().Tint(0xA00F10).XP(1).Variants(3);
		FDefBuilder(TEXT("tropical_fish"), C::WaterAmbient, A::Swimmer, 0.5f, 0.4f, 3, 1.f).Swimmer().NoBreed().NoBaby().Tint(0xEF6915).XP(1).Variants(12);
		FDefBuilder(TEXT("pufferfish"), C::WaterAmbient, A::Swimmer, 0.7f, 0.7f, 3, 1.f).Swimmer().NoBreed().NoBaby().Tint(0xF6B201).XP(1);
		FDefBuilder(TEXT("squid"), C::WaterCreature, A::Swimmer, 0.8f, 0.8f, 10, 1.f).Swimmer().NoBreed().Tint(0x223B4D);
		FDefBuilder(TEXT("glow_squid"), C::Underground, A::Swimmer, 0.8f, 0.8f, 10, 1.f).Swimmer().NoBreed().Tint(0x095656);
		FDefBuilder(TEXT("dolphin"), C::WaterCreature, A::Swimmer, 0.9f, 0.6f, 10, 1.2f).Swimmer().Neutral().NoBreed().Dmg(3).Tint(0x223B4D);
		FDefBuilder(TEXT("axolotl"), C::Underground, A::Swimmer, 0.75f, 0.42f, 14, 1.f).Amphibious().Breed({ TEXT("tropical_fish_bucket") }).Dmg(2).Tint(0xFBC1E3).Variants(5);
		FDefBuilder(TEXT("guardian"), C::Monster, A::Swimmer, 0.85f, 0.85f, 30, 0.5f).Swimmer().Hostile().NoBaby().Dmg(6).XP(10).Tint(0x5A8272);
		FDefBuilder(TEXT("elder_guardian"), C::Monster, A::Swimmer, 1.9975f, 1.9975f, 80, 0.3f).Swimmer().Hostile().NoBaby().Dmg(8).XP(10).Tint(0xCECDC6);
		// ------------------------------------------------------------------ ambient / utility
		FDefBuilder(TEXT("bat"), C::Ambient, A::Flyer, 0.5f, 0.9f, 6, 0.1f).Flying().NoBreed().NoBaby().Tint(0x4C3E30).XP(0);
		FDefBuilder(TEXT("allay"), C::Creature, A::Flyer, 0.35f, 0.6f, 20, 0.1f).Flying().NoBreed().NoBaby().Tint(0x00DAFF).XP(0);
		FDefBuilder(TEXT("villager"), C::Creature, A::Villager, 0.6f, 1.95f, 20, 0.5f).NoBreed().Tint(0x563C33).Eye(1.62f).Variants(7);
		FDefBuilder(TEXT("wandering_trader"), C::Creature, A::Villager, 0.6f, 1.95f, 20, 0.5f).NoBreed().NoBaby().Tint(0x456296).Eye(1.62f);
		FDefBuilder(TEXT("iron_golem"), C::Misc, A::Golem, 1.4f, 2.7f, 100, 0.25f).NoBreed().NoBaby().Dmg(15).KB(1.f).Tint(0xDBCDC2).XP(0);
		FDefBuilder(TEXT("snow_golem"), C::Misc, A::Golem, 0.7f, 1.9f, 4, 0.2f).NoBreed().NoBaby().Tint(0xFFFFFF).XP(0);
		FDefBuilder(TEXT("copper_golem"), C::Misc, A::Golem, 0.49f, 0.98f, 12, 0.2f).NoBreed().NoBaby().Tint(0xC46C4B).XP(0).Variants(4);
		// ------------------------------------------------------------------ overworld monsters
		FDefBuilder(TEXT("zombie"), C::Monster, A::Zombie, 0.6f, 1.95f, 20, 0.23f).Dmg(3).Armor(2).Range(35).Undead().Burns().Eye(1.74f).Tint(0x00AFAF);
		FDefBuilder(TEXT("husk"), C::Monster, A::Zombie, 0.6f, 1.95f, 20, 0.23f).Dmg(3).Armor(2).Range(35).Undead().Eye(1.74f).Tint(0x797061);
		FDefBuilder(TEXT("drowned"), C::Monster, A::Zombie, 0.6f, 1.95f, 20, 0.23f).Dmg(3).Armor(2).Range(35).Undead().Burns().Amphibious().Eye(1.74f).Tint(0x8FF1D7);
		FDefBuilder(TEXT("zombie_villager"), C::Monster, A::Zombie, 0.6f, 1.95f, 20, 0.23f).Dmg(3).Armor(2).Range(35).Undead().Burns().Eye(1.74f).Tint(0x563C33);
		FDefBuilder(TEXT("skeleton"), C::Monster, A::Skeleton, 0.6f, 1.99f, 20, 0.25f).Undead().Burns().Eye(1.74f).Tint(0xC1C1C1);
		FDefBuilder(TEXT("stray"), C::Monster, A::Skeleton, 0.6f, 1.99f, 20, 0.25f).Undead().Burns().Eye(1.74f).Tint(0x617677);
		FDefBuilder(TEXT("bogged"), C::Monster, A::Skeleton, 0.6f, 1.99f, 16, 0.25f).Undead().Burns().Eye(1.74f).Tint(0x8A9F76);
		FDefBuilder(TEXT("parched"), C::Monster, A::Skeleton, 0.6f, 1.99f, 16, 0.25f).Undead().Eye(1.74f).Tint(0xC4A673);
		FDefBuilder(TEXT("creeper"), C::Monster, A::Creeper, 0.6f, 1.7f, 20, 0.25f).Tint(0x0DA70B).Eye(1.44f);
		FDefBuilder(TEXT("spider"), C::Monster, A::Spider, 1.4f, 0.9f, 16, 0.3f).Dmg(2).Arthropod().Tint(0x342D27).Eye(0.65f);
		FDefBuilder(TEXT("cave_spider"), C::Monster, A::Spider, 0.7f, 0.5f, 12, 0.3f).Dmg(2).Arthropod().Tint(0x0C424E).Eye(0.45f);
		FDefBuilder(TEXT("enderman"), C::Monster, A::Enderman, 0.6f, 2.9f, 40, 0.3f).Dmg(7).Neutral().Range(64).Tint(0x161616).Eye(2.55f).Step(1.0f);
		FDefBuilder(TEXT("slime"), C::Monster, A::Slime, 0.52f, 0.52f, 1, 0.3f).Tint(0x51A03E).NoBaby();
		FDefBuilder(TEXT("witch"), C::Monster, A::Illager, 0.6f, 1.95f, 26, 0.25f).Tint(0x340000).Eye(1.62f).NoBaby();
		FDefBuilder(TEXT("silverfish"), C::Monster, A::Silverfish, 0.4f, 0.3f, 8, 0.25f).Dmg(1).Arthropod().Tint(0x6E6E6E).NoBaby();
		FDefBuilder(TEXT("endermite"), C::Monster, A::Silverfish, 0.4f, 0.3f, 8, 0.25f).Dmg(2).Arthropod().Tint(0x161616).NoBaby().XP(3);
		FDefBuilder(TEXT("phantom"), C::Monster, A::Flyer, 0.9f, 0.5f, 20, 0.6f).Flying().Undead().Burns().Dmg(6).Tint(0x43518A).NoBaby();
		FDefBuilder(TEXT("vindicator"), C::Monster, A::Illager, 0.6f, 1.95f, 24, 0.35f).Dmg(13).Tint(0x959B9B).Eye(1.62f).NoBaby();
		FDefBuilder(TEXT("evoker"), C::Monster, A::Illager, 0.6f, 1.95f, 24, 0.5f).Tint(0x959B9B).Eye(1.62f).NoBaby().XP(10);
		FDefBuilder(TEXT("pillager"), C::Monster, A::Skeleton, 0.6f, 1.95f, 24, 0.35f).Tint(0x532F36).Eye(1.62f).NoBaby();
		FDefBuilder(TEXT("ravager"), C::Monster, A::Illager, 1.95f, 2.2f, 100, 0.3f).Dmg(12).KB(0.75f).Tint(0x757470).NoBaby().XP(20).Step(1.0f);
		FDefBuilder(TEXT("vex"), C::Monster, A::Flyer, 0.4f, 0.8f, 14, 0.2f).Flying().Dmg(9).Tint(0x7A90A4).NoBaby().XP(3);
		FDefBuilder(TEXT("breeze"), C::Monster, A::Blaze, 0.6f, 1.77f, 30, 0.63f).Tint(0xAF94DF).NoBaby().XP(10);
		FDefBuilder(TEXT("creaking"), C::Monster, A::Zombie, 0.9f, 2.7f, 1, 0.4f).Dmg(3).Tint(0x5F5F5F).NoBaby().XP(0);
		FDefBuilder(TEXT("warden"), C::Monster, A::Warden, 0.9f, 2.9f, 500, 0.3f).Dmg(30).KB(1.f).Range(24).Tint(0x0F4649).NoBaby().XP(5).Step(1.0f);
		FDefBuilder(TEXT("sulfur_cube"), C::Underground, A::Slime, 0.9f, 0.9f, 8, 0.25f).Neutral().Tint(0xD8C13C).NoBaby().Name(TEXT("Sulfur Cube"));
		// ------------------------------------------------------------------ nether
		FDefBuilder(TEXT("ghast"), C::Monster, A::Ghast, 4.f, 4.f, 10, 0.05f).Flying().FireImmune().Range(64).Tint(0xF9F9F9).NoBaby().XP(5).Eye(2.6f);
		FDefBuilder(TEXT("blaze"), C::Monster, A::Blaze, 0.6f, 1.8f, 20, 0.23f).FireImmune().Dmg(6).Range(48).Tint(0xF6B201).NoBaby().XP(10);
		FDefBuilder(TEXT("magma_cube"), C::Monster, A::Slime, 0.52f, 0.52f, 1, 0.3f).FireImmune().Tint(0x340000).NoBaby();
		FDefBuilder(TEXT("piglin"), C::Monster, A::Zombie, 0.6f, 1.95f, 16, 0.35f).Neutral().Dmg(5).Tint(0x995640).Eye(1.79f);
		FDefBuilder(TEXT("piglin_brute"), C::Monster, A::Zombie, 0.6f, 1.95f, 50, 0.35f).Dmg(7).Tint(0x592A10).Eye(1.79f).NoBaby().XP(20);
		FDefBuilder(TEXT("zombified_piglin"), C::Monster, A::Zombie, 0.6f, 1.95f, 20, 0.23f).Neutral().Dmg(5).Armor(2).Undead().FireImmune().Tint(0xEA9393).Eye(1.79f);
		FDefBuilder(TEXT("hoglin"), C::Monster, A::Zombie, 1.3964844f, 1.4f, 40, 0.3f).Dmg(6).KB(0.6f).Breed({ TEXT("crimson_fungus") }).Tint(0xC66E55);
		FDefBuilder(TEXT("zoglin"), C::Monster, A::Zombie, 1.3964844f, 1.4f, 40, 0.3f).Dmg(6).KB(0.6f).Undead().Tint(0xC66E55);
		FDefBuilder(TEXT("wither_skeleton"), C::Monster, A::Zombie, 0.7f, 2.4f, 20, 0.25f).Dmg(8).Undead().FireImmune().Tint(0x141414).Eye(2.1f);
		// ------------------------------------------------------------------ end
		FDefBuilder(TEXT("shulker"), C::Monster, A::Shulker, 1.f, 1.f, 30, 0.f).Armor(20).KB(1.f).Tint(0x946794).NoBaby().XP(5).Variants(17);
		// ------------------------------------------------------------------ bosses
		FDefBuilder(TEXT("ender_dragon"), C::Boss, A::Dragon, 16.f, 8.f, 200, 0.f).Boss().FireImmune().Flying().Tint(0x1C1C1C).XP(12000).Name(TEXT("Ender Dragon")).Scale(4.2f);
		FDefBuilder(TEXT("wither"), C::Boss, A::Wither, 0.9f, 3.5f, 300, 0.6f).Boss().FireImmune().Undead().Flying().Armor(4).Tint(0x141414).XP(50).Scale(1.45f);
	}
}

namespace MCMobs
{
	void Init()
	{
		if (GMobsInit) return;
		GMobsInit = true;
		GMobs.Reserve(100);
		RegisterAll();
		UE_LOG(LogOpus55, Log, TEXT("Registered %d mob types"), GMobs.Num());
	}
	const FMCMobDef* Find(FName Id)
	{
		Init();
		const int32* I = GMobIndex.Find(Id);
		return I ? &GMobs[*I] : nullptr;
	}
	const TArray<FMCMobDef>& All()
	{
		Init();
		return GMobs;
	}
}
