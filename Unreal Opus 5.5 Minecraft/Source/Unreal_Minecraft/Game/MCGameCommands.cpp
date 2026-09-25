// Chat commands (Java syntax subset): gamemode, give, summon, time, weather, tp, locate, kill, difficulty, effect,
// enchant, xp, clear, setblock, fill, gamerule, seed, spawnpoint, setworldspawn, say, help... plus tab completion.
// Coordinates are entered in Minecraft order (x, y = height, z) and converted to the internal Z-up layout.
#include "Game/MCGame.h"
#include "Game/MCPlayer.h"
#include "Game/MCMob.h"
#include "Game/MCEntities.h"
#include "World/MCWorld.h"
#include "Gen/MCWorldGen.h"
#include "Gen/MCBiomes.h"

namespace
{
	const TCHAR* GCommands[] = {
		TEXT("clear"), TEXT("defaultgamemode"), TEXT("difficulty"), TEXT("effect"), TEXT("enchant"), TEXT("experience"), TEXT("fill"),
		TEXT("gamemode"), TEXT("gamerule"), TEXT("give"), TEXT("help"), TEXT("kill"), TEXT("locate"), TEXT("particle"), TEXT("save-all"),
		TEXT("say"), TEXT("seed"), TEXT("setblock"), TEXT("setworldspawn"), TEXT("spawnpoint"), TEXT("summon"), TEXT("teleport"),
		TEXT("time"), TEXT("tp"), TEXT("weather"), TEXT("xp")
	};
	const TCHAR* GGameRules[] = {
		TEXT("doDaylightCycle"), TEXT("doWeatherCycle"), TEXT("doMobSpawning"), TEXT("keepInventory"), TEXT("mobGriefing"),
		TEXT("doFireTick"), TEXT("naturalRegeneration"), TEXT("showCoordinates"), TEXT("doImmediateRespawn"), TEXT("fallDamage"),
		TEXT("drowningDamage"), TEXT("fireDamage"), TEXT("doTileDrops"), TEXT("doMobLoot"), TEXT("randomTickSpeed"), TEXT("spawnRadius")
	};
	const TCHAR* GOtherEntities[] = {
		TEXT("tnt"), TEXT("lightning_bolt"), TEXT("end_crystal"), TEXT("boat"), TEXT("minecart"), TEXT("chest_minecart"),
		TEXT("furnace_minecart"), TEXT("tnt_minecart"), TEXT("hopper_minecart"), TEXT("experience_orb"), TEXT("arrow"), TEXT("fireball"),
		TEXT("small_fireball"), TEXT("wither_skull"), TEXT("dragon_fireball"), TEXT("snowball"), TEXT("firework_rocket"), TEXT("wind_charge")
	};

	FString StripNs(const FString& S) { return S.StartsWith(TEXT("minecraft:")) ? S.Mid(10) : S; }
	FString Err(const FString& S) { return TEXT("§c") + S; }
	FString NormRule(const FString& S) { return S.Replace(TEXT("_"), TEXT("")).ToLower(); }

	TArray<FString> Tokenize(const FString& Line)
	{
		TArray<FString> Out;
		FString Cur;
		int32 Bracket = 0;
		bool bQuote = false;
		for (TCHAR C : Line)
		{
			if (C == '"') { bQuote = !bQuote; continue; }
			if (C == '[') ++Bracket;
			if (C == ']') --Bracket;
			if (FChar::IsWhitespace(C) && !bQuote && Bracket <= 0) { if (!Cur.IsEmpty()) Out.Add(Cur); Cur.Reset(); continue; }
			Cur.AppendChar(C);
		}
		if (!Cur.IsEmpty()) Out.Add(Cur);
		return Out;
	}

	bool ParseNum(const FString& T, double& Out) { return !T.IsEmpty() && LexTryParseString(Out, *T); }
	bool ParseInt(const FString& T, int32& Out) { double D; if (!ParseNum(T, D)) return false; Out = (int32)D; return true; }

	/** Duration token with optional suffix (t ticks, s seconds, d days). Returns ticks. */
	bool ParseDuration(const FString& T, int32 DefaultUnitTicks, int32& Out)
	{
		if (T.IsEmpty()) return false;
		int32 Mul = DefaultUnitTicks;
		FString N = T;
		if (T.EndsWith(TEXT("t"))) { Mul = 1; N = T.LeftChop(1); }
		else if (T.EndsWith(TEXT("s"))) { Mul = 20; N = T.LeftChop(1); }
		else if (T.EndsWith(TEXT("d"))) { Mul = 24000; N = T.LeftChop(1); }
		double V; if (!ParseNum(N, V)) return false;
		Out = (int32)(V * Mul);
		return true;
	}

	FString ModeName(EMCGameMode M)
	{
		switch (M) { case EMCGameMode::Survival: return TEXT("Survival Mode"); case EMCGameMode::Creative: return TEXT("Creative Mode"); case EMCGameMode::Adventure: return TEXT("Adventure Mode"); default: return TEXT("Spectator Mode"); }
	}
	bool ParseMode(const FString& S, EMCGameMode& Out)
	{
		const FString L = S.ToLower();
		if (L == TEXT("survival") || L == TEXT("s") || L == TEXT("0")) Out = EMCGameMode::Survival;
		else if (L == TEXT("creative") || L == TEXT("c") || L == TEXT("1")) Out = EMCGameMode::Creative;
		else if (L == TEXT("adventure") || L == TEXT("a") || L == TEXT("2")) Out = EMCGameMode::Adventure;
		else if (L == TEXT("spectator") || L == TEXT("sp") || L == TEXT("3")) Out = EMCGameMode::Spectator;
		else return false;
		return true;
	}

	FString EntityTypeName(const AMCEntity* E)
	{
		if (const AMCMob* M = Cast<AMCMob>(E)) if (M->Def) return M->Def->Id.ToString();
		if (E->IsA<AMCPlayer>()) return TEXT("player");
		return E->TypeId.IsNone() ? E->GetClass()->GetName() : E->TypeId.ToString();
	}

	FString CoordText(const FVector& P) { return FString::Printf(TEXT("%.2f, %.2f, %.2f"), P.X, P.Z, P.Y); }
	FString BlockText(const FMCBlockPos& P) { return FString::Printf(TEXT("%d, %d, %d"), P.X, P.Z, P.Y); }
}

struct FMCCommandContext
{
	AMCGame* Game = nullptr;
	AMCPlayer* Source = nullptr;
	FMCWorld* World = nullptr;
	TArray<FString> Args;

	FVector Origin() const { return Source ? Source->Pos : (Game ? FVector(Game->WorldSpawn.X + 0.5, Game->WorldSpawn.Y + 0.5, Game->WorldSpawn.Z) : FVector::ZeroVector); }
	void Reply(const FString& S) const { Game->AddChat(S); }
	bool Fail(const FString& S) const { Game->AddChat(Err(S)); return false; }

	/** Reads x y z (Minecraft order) starting at Index. Supports ~ relative and ^ local coordinates. */
	bool ParsePos(int32 Index, FVector& Out, bool bCenter) const
	{
		if (!Args.IsValidIndex(Index + 2)) return false;
		const FVector O = Origin();
		const FString& A = Args[Index]; const FString& B = Args[Index + 1]; const FString& C = Args[Index + 2];
		if (A.StartsWith(TEXT("^")) && B.StartsWith(TEXT("^")) && C.StartsWith(TEXT("^")))
		{
			double L = 0, U = 0, F = 0;
			if (A.Len() > 1 && !ParseNum(A.Mid(1), L)) return false;
			if (B.Len() > 1 && !ParseNum(B.Mid(1), U)) return false;
			if (C.Len() > 1 && !ParseNum(C.Mid(1), F)) return false;
			const float Yaw = Source ? Source->Yaw : 0.f, Pitch = Source ? Source->Pitch : 0.f;
			const FVector Fwd = FRotator(Pitch, Yaw, 0).Vector();
			const FVector Right = FRotator(0, Yaw + 90.f, 0).Vector();
			const FVector Up = FVector::CrossProduct(Right, Fwd).GetSafeNormal();
			Out = (Source ? Source->GetEyePos() : O) + Fwd * F - Right * L + Up * U;
			return true;
		}
		auto Axis = [&](const FString& T, double Base, bool bHoriz, double& V) -> bool
		{
			if (T.StartsWith(TEXT("~")))
			{
				double D = 0;
				if (T.Len() > 1 && !ParseNum(T.Mid(1), D)) return false;
				V = Base + D;
				return true;
			}
			if (!ParseNum(T, V)) return false;
			if (bHoriz && bCenter && !T.Contains(TEXT("."))) V += 0.5;
			return true;
		};
		double X, Height, Z;
		if (!Axis(A, O.X, true, X) || !Axis(B, O.Z, false, Height) || !Axis(C, O.Y, true, Z)) return false;
		Out = FVector(X, Z, Height);
		return true;
	}
	bool ParseBlockPos(int32 Index, FMCBlockPos& Out) const
	{
		FVector V;
		if (!ParsePos(Index, V, false)) return false;
		Out = FMCBlockPos(MC::FloorToInt(V.X), MC::FloorToInt(V.Y), MC::FloorToInt(V.Z));
		return true;
	}

	/** Target selectors: @s @p @a @r @e[type=..,distance=..N,limit=N] or the player name. */
	bool ResolveTargets(const FString& Token, TArray<AMCEntity*>& Out) const
	{
		const FString T = Token.IsEmpty() ? TEXT("@s") : Token;
		if (T == TEXT("@s") || T == TEXT("@p") || T == TEXT("@a") || T == TEXT("@r") || T.Equals(TEXT("Steve"), ESearchCase::IgnoreCase) || T.StartsWith(TEXT("@p[")) || T.StartsWith(TEXT("@a[")))
		{
			if (Game->Player) Out.Add(Game->Player);
			return Out.Num() > 0;
		}
		if (!T.StartsWith(TEXT("@e")) || !World) return false;
		FString TypeFilter; bool bNegate = false; double MaxDist = -1; int32 Limit = INT_MAX;
		if (T.Len() > 3 && T[2] == '[')
		{
			TArray<FString> Parts;
			T.Mid(3, T.Len() - 4).ParseIntoArray(Parts, TEXT(","));
			for (const FString& P : Parts)
			{
				FString K, V;
				if (!P.Split(TEXT("="), &K, &V)) continue;
				K.TrimStartAndEndInline(); V.TrimStartAndEndInline();
				if (K == TEXT("type")) { bNegate = V.StartsWith(TEXT("!")); TypeFilter = StripNs(bNegate ? V.Mid(1) : V); }
				else if (K == TEXT("distance")) { FString Num = V.Replace(TEXT(".."), TEXT("")); ParseNum(Num, MaxDist); }
				else if (K == TEXT("limit")) ParseInt(V, Limit);
			}
		}
		const FVector O = Origin();
		TArray<AMCEntity*> Found;
		for (AMCEntity* E : World->Entities)
		{
			if (!E || E->bRemoved) continue;
			if (!TypeFilter.IsEmpty() && (EntityTypeName(E) == TypeFilter) == bNegate) continue;
			if (MaxDist >= 0 && FVector::Dist(E->Pos, O) > MaxDist) continue;
			Found.Add(E);
		}
		if (Game->Player && Game->Player->World == World && !Found.Contains(Game->Player) && (TypeFilter.IsEmpty() ? true : ((TypeFilter == TEXT("player")) != bNegate)))
			if (MaxDist < 0 || FVector::Dist(Game->Player->Pos, O) <= MaxDist) Found.Add(Game->Player);
		Found.Sort([&](const AMCEntity& A, const AMCEntity& B) { return FVector::DistSquared(A.Pos, O) < FVector::DistSquared(B.Pos, O); });
		for (AMCEntity* E : Found) { if (Out.Num() >= Limit) break; Out.Add(E); }
		return Out.Num() > 0;
	}
	AMCPlayer* ResolvePlayer(int32 Index) const
	{
		if (!Args.IsValidIndex(Index)) return Source ? Source : Game->Player.Get();
		TArray<AMCEntity*> T;
		if (!ResolveTargets(Args[Index], T)) return nullptr;
		for (AMCEntity* E : T) if (AMCPlayer* P = Cast<AMCPlayer>(E)) return P;
		return nullptr;
	}
};

namespace
{
	bool CmdGive(FMCCommandContext& C)
	{
		if (C.Args.Num() < 3) return C.Fail(TEXT("Usage: /give <target> <item> [count]"));
		AMCPlayer* P = C.ResolvePlayer(1);
		if (!P) return C.Fail(TEXT("No player was found"));
		const FString Name = StripNs(C.Args[2]);
		const FMCItem* Item = FMCItems::Find(FName(*Name));
		if (!Item || Item->Id == 0) return C.Fail(FString::Printf(TEXT("Unknown item '%s'"), *Name));
		int32 Count = 1;
		if (C.Args.IsValidIndex(3) && (!ParseInt(C.Args[3], Count) || Count < 1 || Count > 6400)) return C.Fail(TEXT("Invalid count"));
		int32 Left = Count;
		while (Left > 0)
		{
			const int32 N = FMath::Min(Left, FMath::Max(1, Item->MaxStack));
			P->GiveItem(FMCItemStack(Item->Id, N));
			Left -= N;
		}
		P->PlaySound(TEXT("entity_item_pickup"), 0.2f, 1.4f);
		C.Reply(FString::Printf(TEXT("Gave %d [%s] to %s"), Count, *Item->DisplayName, *P->GetDisplayName()));
		return true;
	}

	bool CmdSummon(FMCCommandContext& C)
	{
		if (C.Args.Num() < 2) return C.Fail(TEXT("Usage: /summon <entity> [x y z]"));
		const FString Id = StripNs(C.Args[1]);
		FVector Pos = C.Origin();
		if (C.Args.Num() >= 5 && !C.ParsePos(2, Pos, true)) return C.Fail(TEXT("Invalid position"));
		FMCWorld* W = C.World;
		if (!W) return C.Fail(TEXT("No world loaded"));
		AMCGame* G = C.Game;
		AMCEntity* Spawned = nullptr;
		// optional entity tag compound, e.g. {NoAI:1b,PersistenceRequired:1b,CustomName:"Bob"}
		FString Tags;
		for (int32 i = 2; i < C.Args.Num(); ++i) if (C.Args[i].StartsWith(TEXT("{"))) Tags = C.Args[i].ToLower();
		const bool bNoAI = Tags.Contains(TEXT("noai:1")) || Tags.Contains(TEXT("noai:true"));
		const bool bSilent = Tags.Contains(TEXT("silent:1")) || Tags.Contains(TEXT("silent:true"));
		if (MCMobs::Find(FName(*Id)))
		{
			Spawned = G->SpawnMob(W, FName(*Id), Pos, false);
			if (AMCMob* M = Cast<AMCMob>(Spawned))
			{
				M->bPersistent = true;
				M->bNoAI = bNoAI;
				M->bSilent = bSilent;
				if (C.Source && bNoAI) { M->Yaw = M->BodyYaw = M->HeadYaw = C.Source->Yaw + 180.f; M->PrevYaw = M->PrevBodyYaw = M->Yaw; }
			}
		}
		else if (Id == TEXT("lightning_bolt")) { G->StrikeLightning(W, Pos, false); C.Reply(TEXT("Summoned new Lightning Bolt")); return true; }
		else if (Id == TEXT("experience_orb")) { W->SpawnXP(Pos, 7); C.Reply(TEXT("Summoned new Experience Orb")); return true; }
		else if (Id == TEXT("tnt")) { if (AMCPrimedTNT* T = G->SpawnEntity<AMCPrimedTNT>(W, Pos)) { T->InitEntity(); Spawned = T; } }
		else if (Id == TEXT("end_crystal")) { if (AMCEndCrystal* E = G->SpawnEntity<AMCEndCrystal>(W, Pos)) { E->InitEntity(); Spawned = E; } }
		else if (Id.EndsWith(TEXT("boat")) || Id.EndsWith(TEXT("raft")))
		{
			if (AMCBoat* B = G->SpawnEntity<AMCBoat>(W, Pos))
			{
				FString Wood = Id; Wood.RemoveFromEnd(TEXT("_chest_boat")); Wood.RemoveFromEnd(TEXT("_boat")); Wood.RemoveFromEnd(TEXT("_chest_raft")); Wood.RemoveFromEnd(TEXT("_raft"));
				if (Wood != TEXT("boat")) B->Wood = FName(*Wood);
				B->bChest = Id.Contains(TEXT("chest"));
				B->Yaw = C.Source ? C.Source->Yaw : 0.f;
				B->InitEntity(); Spawned = B;
			}
		}
		else if (Id.EndsWith(TEXT("minecart"))) { if (AMCMinecart* M = G->SpawnEntity<AMCMinecart>(W, Pos)) { M->Variant = FName(*Id); M->InitEntity(); Spawned = M; } }
		else if (Id == TEXT("firework_rocket")) { if (AMCFirework* F = G->SpawnEntity<AMCFirework>(W, Pos)) { F->Item = FMCItemStack::Of(TEXT("firework_rocket")); F->Vel = FVector(0, 0, 0.05); F->InitEntity(); Spawned = F; } }
		else
		{
			static const TMap<FString, EMCProjectile> Proj = {
				{ TEXT("arrow"), EMCProjectile::Arrow }, { TEXT("spectral_arrow"), EMCProjectile::SpectralArrow }, { TEXT("fireball"), EMCProjectile::Fireball },
				{ TEXT("small_fireball"), EMCProjectile::SmallFireball }, { TEXT("wither_skull"), EMCProjectile::WitherSkull }, { TEXT("dragon_fireball"), EMCProjectile::DragonFireball },
				{ TEXT("snowball"), EMCProjectile::Snowball }, { TEXT("egg"), EMCProjectile::Egg }, { TEXT("wind_charge"), EMCProjectile::WindCharge }, { TEXT("trident"), EMCProjectile::Trident }
			};
			if (const EMCProjectile* T = Proj.Find(Id))
			{
				if (AMCProjectile* P = G->SpawnEntity<AMCProjectile>(W, Pos))
				{
					P->Type = *T;
					if (*T == EMCProjectile::Trident) P->Item = FMCItemStack::Of(TEXT("trident"));
					P->bPickup = false;
					P->InitEntity();
					Spawned = P;
				}
			}
		}
		if (!Spawned) return C.Fail(FString::Printf(TEXT("Unable to summon entity '%s'"), *Id));
		C.Reply(FString::Printf(TEXT("Summoned new %s"), *Spawned->GetDisplayName()));
		return true;
	}

	bool CmdTime(FMCCommandContext& C)
	{
		AMCGame* G = C.Game;
		if (C.Args.Num() < 2) return C.Fail(TEXT("Usage: /time <set|add|query> <value>"));
		const FString Op = C.Args[1].ToLower();
		if (Op == TEXT("query"))
		{
			const FString What = C.Args.IsValidIndex(2) ? C.Args[2].ToLower() : TEXT("daytime");
			const int64 V = What == TEXT("gametime") ? G->GameTime : (What == TEXT("day") ? G->DayTime / 24000 : G->DayTime % 24000);
			C.Reply(FString::Printf(TEXT("The time is %lld"), V));
			return true;
		}
		if (!C.Args.IsValidIndex(2)) return C.Fail(TEXT("Missing time value"));
		const FString V = C.Args[2].ToLower();
		int32 Ticks = 0;
		if (V == TEXT("day")) Ticks = 1000; else if (V == TEXT("noon")) Ticks = 6000; else if (V == TEXT("night")) Ticks = 13000;
		else if (V == TEXT("midnight")) Ticks = 18000; else if (V == TEXT("sunrise")) Ticks = 23000; else if (V == TEXT("sunset")) Ticks = 12000;
		else if (!ParseDuration(V, 1, Ticks)) return C.Fail(FString::Printf(TEXT("Invalid time '%s'"), *V));
		if (Op == TEXT("set")) G->SetTime((G->DayTime / 24000) * 24000 + Ticks);
		else if (Op == TEXT("add")) G->SetTime(G->DayTime + Ticks);
		else return C.Fail(TEXT("Usage: /time <set|add|query> <value>"));
		C.Reply(FString::Printf(TEXT("Set the time to %lld"), G->DayTime % 24000));
		return true;
	}

	bool CmdWeather(FMCCommandContext& C)
	{
		if (C.Args.Num() < 2) return C.Fail(TEXT("Usage: /weather <clear|rain|thunder> [duration]"));
		const FString K = C.Args[1].ToLower();
		int32 Dur = -1;
		if (C.Args.IsValidIndex(2) && !ParseDuration(C.Args[2], 20, Dur)) return C.Fail(TEXT("Invalid duration"));
		FMCRandom& R = C.Game->Rand;
		if (K == TEXT("clear")) { C.Game->SetWeather(0, Dur > 0 ? Dur : R.Range(12000, 180000)); C.Reply(TEXT("Set the weather to clear")); }
		else if (K == TEXT("rain")) { C.Game->SetWeather(1, Dur > 0 ? Dur : R.Range(12000, 24000)); C.Reply(TEXT("Set the weather to rain")); }
		else if (K == TEXT("thunder")) { C.Game->SetWeather(2, Dur > 0 ? Dur : R.Range(3600, 15600)); C.Reply(TEXT("Set the weather to rain & thunder")); }
		else return C.Fail(FString::Printf(TEXT("Unknown weather '%s'"), *K));
		return true;
	}

	bool CmdTeleport(FMCCommandContext& C)
	{
		const int32 N = C.Args.Num();
		TArray<AMCEntity*> Targets;
		FVector Dest;
		if (N == 4) { if (!C.ParsePos(1, Dest, true)) return C.Fail(TEXT("Invalid position")); if (C.Game->Player) Targets.Add(C.Game->Player); }
		else if (N >= 5) { if (!C.ResolveTargets(C.Args[1], Targets)) return C.Fail(TEXT("No entity was found")); if (!C.ParsePos(2, Dest, true)) return C.Fail(TEXT("Invalid position")); }
		else if (N == 2 || N == 3)
		{
			TArray<AMCEntity*> DestE;
			if (N == 2) { if (C.Game->Player) Targets.Add(C.Game->Player); if (!C.ResolveTargets(C.Args[1], DestE)) return C.Fail(TEXT("No entity was found")); }
			else { if (!C.ResolveTargets(C.Args[1], Targets) || !C.ResolveTargets(C.Args[2], DestE)) return C.Fail(TEXT("No entity was found")); }
			Dest = DestE[0]->Pos;
		}
		else return C.Fail(TEXT("Usage: /tp [targets] <x y z> | /tp <destination>"));
		Dest.Z = FMath::Clamp(Dest.Z, (double)MC::MinZ - 64, (double)MC::MaxZ + 512);
		for (AMCEntity* E : Targets)
		{
			if (E->IsPassenger()) E->StopRiding();
			E->TeleportTo(Dest);
			E->Vel = FVector::ZeroVector;
			E->FallDistance = 0.f;
			if (C.Args.Num() >= 7 && Cast<AMCPlayer>(E)) { double Y, P; if (ParseNum(C.Args[5], Y) && ParseNum(C.Args[6], P)) { E->Yaw = (float)Y + 90.f; E->Pitch = (float)P; } }
		}
		C.Reply(Targets.Num() == 1 ? FString::Printf(TEXT("Teleported %s to %s"), *Targets[0]->GetDisplayName(), *CoordText(Dest)) : FString::Printf(TEXT("Teleported %d entities to %s"), Targets.Num(), *CoordText(Dest)));
		return true;
	}

	bool CmdLocate(FMCCommandContext& C)
	{
		if (C.Args.Num() < 2 || !C.World || !C.World->Generator) return C.Fail(TEXT("Usage: /locate <structure|biome> <id>"));
		FString Kind = C.Args[1].ToLower(), Id;
		if (Kind == TEXT("structure") || Kind == TEXT("biome") || Kind == TEXT("poi")) { if (!C.Args.IsValidIndex(2)) return C.Fail(TEXT("Missing id")); Id = StripNs(C.Args[2]); }
		else { Id = StripNs(C.Args[1]); Kind = TEXT("structure"); }
		const FMCBlockPos Origin = C.Source ? C.Source->BlockPos() : C.Game->WorldSpawn;
		FMCBlockPos Found;
		FString Label = Id;
		if (Kind == TEXT("biome"))
		{
			const EMCBiome B = FMCBiomes::FromName(Id);
			if (B == EMCBiome::Count) return C.Fail(FString::Printf(TEXT("There is no biome with type \"%s\""), *Id));
			if (!C.World->Generator->LocateBiome((uint8)B, Origin, 6400, Found)) return C.Fail(FString::Printf(TEXT("Could not find a biome of type \"%s\" within reasonable distance"), *Id));
		}
		else
		{
			TArray<FName> Names;
			C.World->Generator->GetStructureNames(Names);
			TArray<FName> Candidates;
			for (FName S : Names) if (S.ToString() == Id) Candidates.Add(S);
			if (Candidates.Num() == 0) for (FName S : Names) if (S.ToString().Contains(Id)) Candidates.Add(S);
			if (Candidates.Num() == 0) return C.Fail(FString::Printf(TEXT("There is no structure with type \"%s\" in this dimension"), *Id));
			double Best = DBL_MAX;
			bool bAny = false;
			for (FName S : Candidates)
			{
				FMCBlockPos P;
				if (!C.World->Generator->LocateStructure(S, Origin, 100, P)) continue;
				const double D = FVector2D::Distance(FVector2D(P.X, P.Y), FVector2D(Origin.X, Origin.Y));
				if (D < Best) { Best = D; Found = P; Label = S.ToString(); bAny = true; }
			}
			if (!bAny) return C.Fail(FString::Printf(TEXT("Could not find a structure of type \"%s\" nearby"), *Id));
		}
		const int32 Dist = FMath::RoundToInt(FVector2D::Distance(FVector2D(Found.X, Found.Y), FVector2D(Origin.X, Origin.Y)));
		C.Reply(FString::Printf(TEXT("The nearest %s is at [%d, ~, %d] (%d blocks away)"), *Label, Found.X, Found.Y, Dist));
		return true;
	}

	bool CmdKill(FMCCommandContext& C)
	{
		TArray<AMCEntity*> Targets;
		if (!C.ResolveTargets(C.Args.IsValidIndex(1) ? C.Args[1] : TEXT("@s"), Targets)) return C.Fail(TEXT("No entity was found"));
		for (AMCEntity* E : Targets)
		{
			if (AMCLiving* L = Cast<AMCLiving>(E))
			{
				FMCDamage D = FMCDamage::Of(TEXT("genericKill"));
				D.bBypassArmor = D.bBypassInvulnerability = D.bBypassCreative = true;
				D.bNoKnockback = true;
				L->Hurt(D, 3.4e38f);
				if (L->Health > 0.f) { L->Health = 0.f; L->Die(D); }
			}
			else E->Discard();
		}
		C.Reply(Targets.Num() == 1 ? FString::Printf(TEXT("Killed %s"), *Targets[0]->GetDisplayName()) : FString::Printf(TEXT("Killed %d entities"), Targets.Num()));
		return true;
	}

	bool CmdEffect(FMCCommandContext& C)
	{
		if (C.Args.Num() < 2) return C.Fail(TEXT("Usage: /effect <give|clear> <target> [effect] [seconds] [amplifier]"));
		const FString Op = C.Args[1].ToLower();
		TArray<AMCEntity*> Targets;
		if (!C.ResolveTargets(C.Args.IsValidIndex(2) ? C.Args[2] : TEXT("@s"), Targets)) return C.Fail(TEXT("No entity was found"));
		if (Op == TEXT("clear"))
		{
			const EMCEffect E = C.Args.IsValidIndex(3) ? MCEffects::FromName(StripNs(C.Args[3])) : EMCEffect::None;
			for (AMCEntity* T : Targets) if (AMCLiving* L = Cast<AMCLiving>(T)) { if (E == EMCEffect::None) L->ClearEffects(); else L->RemoveEffect(E); }
			C.Reply(FString::Printf(TEXT("Removed effects from %d target(s)"), Targets.Num()));
			return true;
		}
		if (Op != TEXT("give") || !C.Args.IsValidIndex(3)) return C.Fail(TEXT("Usage: /effect give <target> <effect> [seconds] [amplifier] [hideParticles]"));
		const EMCEffect E = MCEffects::FromName(StripNs(C.Args[3]));
		if (E == EMCEffect::None) return C.Fail(FString::Printf(TEXT("Unknown effect '%s'"), *C.Args[3]));
		int32 Seconds = 30, Amp = 0;
		if (C.Args.IsValidIndex(4)) { if (C.Args[4] == TEXT("infinite")) Seconds = 1000000; else if (!ParseInt(C.Args[4], Seconds)) return C.Fail(TEXT("Invalid duration")); }
		if (C.Args.IsValidIndex(5) && !ParseInt(C.Args[5], Amp)) return C.Fail(TEXT("Invalid amplifier"));
		const bool bHide = C.Args.IsValidIndex(6) && C.Args[6].ToLower() == TEXT("true");
		int32 Applied = 0;
		for (AMCEntity* T : Targets)
		{
			AMCLiving* L = Cast<AMCLiving>(T);
			if (!L) continue;
			if (MCEffects::IsInstant(E)) { L->ApplyInstantEffect(E, Amp, C.Source); ++Applied; continue; }
			FMCEffectInstance I; I.Effect = E; I.Duration = Seconds * 20; I.Amplifier = (uint8)FMath::Clamp(Amp, 0, 255); I.bShowParticles = !bHide;
			if (L->AddEffect(I)) ++Applied;
		}
		C.Reply(FString::Printf(TEXT("Applied effect %s to %d target(s)"), MCEffects::Name(E), Applied));
		return Applied > 0;
	}

	bool CmdEnchant(FMCCommandContext& C)
	{
		if (C.Args.Num() < 3) return C.Fail(TEXT("Usage: /enchant <target> <enchantment> [level]"));
		AMCPlayer* P = C.ResolvePlayer(1);
		if (!P) return C.Fail(TEXT("No player was found"));
		const EMCEnchant E = MCEnchants::FromName(StripNs(C.Args[2]));
		if (E == EMCEnchant::None) return C.Fail(FString::Printf(TEXT("Unknown enchantment '%s'"), *C.Args[2]));
		int32 Level = 1;
		if (C.Args.IsValidIndex(3) && !ParseInt(C.Args[3], Level)) return C.Fail(TEXT("Invalid level"));
		const MCEnchants::FInfo& Info = MCEnchants::Info(E);
		if (Level > Info.MaxLevel) return C.Fail(FString::Printf(TEXT("%d is higher than the maximum level of %d supported by that enchantment"), Level, Info.MaxLevel));
		FMCItemStack& S = P->Held();
		if (S.IsEmpty()) return C.Fail(TEXT("Steve is not holding any item"));
		if (!MCEnchants::CanApply(E, S.Item())) return C.Fail(FString::Printf(TEXT("%s cannot support that enchantment"), *S.GetDisplayName()));
		if (S.HasExtra()) for (const FMCEnchantLevel& L : S.Extra->Enchants) if (L.Id != E && !MCEnchants::Compatible(L.Id, E)) return C.Fail(TEXT("That enchantment conflicts with an existing enchantment"));
		S.AddEnchant(E, (uint8)Level);
		C.Reply(FString::Printf(TEXT("Applied enchantment %s %s to %s's item"), Info.Name, *MCEnchants::Roman(Level), *P->GetDisplayName()));
		return true;
	}

	bool CmdXP(FMCCommandContext& C)
	{
		if (C.Args.Num() < 3) return C.Fail(TEXT("Usage: /xp <add|set|query> <target> [amount] [points|levels]"));
		const FString Op = C.Args[1].ToLower();
		AMCPlayer* P = C.ResolvePlayer(2);
		if (!P) return C.Fail(TEXT("No player was found"));
		if (Op == TEXT("query")) { C.Reply(FString::Printf(TEXT("%s has %d experience levels (%d points)"), *P->GetDisplayName(), P->XPLevel, P->XPTotal)); return true; }
		int32 Amount = 0;
		if (!C.Args.IsValidIndex(3) || !ParseInt(C.Args[3].Replace(TEXT("L"), TEXT("")), Amount)) return C.Fail(TEXT("Invalid amount"));
		const bool bLevels = (C.Args.IsValidIndex(4) && C.Args[4].ToLower().StartsWith(TEXT("level"))) || C.Args[3].EndsWith(TEXT("L"));
		if (Op == TEXT("set")) { if (bLevels) { P->XPLevel = FMath::Max(0, Amount); P->XPProgress = 0.f; } else { P->XPLevel = 0; P->XPProgress = 0.f; P->XPTotal = 0; P->GiveXP(FMath::Max(0, Amount)); } }
		else if (bLevels) P->GiveXPLevels(Amount);
		else P->GiveXP(Amount);
		C.Reply(FString::Printf(TEXT("Gave %d experience %s to %s"), Amount, bLevels ? TEXT("levels") : TEXT("points"), *P->GetDisplayName()));
		return true;
	}

	bool CmdClear(FMCCommandContext& C)
	{
		AMCPlayer* P = C.ResolvePlayer(1);
		if (!P) return C.Fail(TEXT("No player was found"));
		FMCItemId Only = 0;
		if (C.Args.IsValidIndex(2)) { Only = FMCItems::FindId(FName(*StripNs(C.Args[2]))); if (!Only) return C.Fail(TEXT("Unknown item")); }
		int32 Removed = 0;
		for (FMCItemStack& S : P->Inventory.Slots) if (!S.IsEmpty() && (!Only || S.Id == Only)) { Removed += S.Count; S.Clear(); }
		if (!P->CarriedStack.IsEmpty() && (!Only || P->CarriedStack.Id == Only)) { Removed += P->CarriedStack.Count; P->CarriedStack.Clear(); }
		if (Removed == 0) return C.Fail(FString::Printf(TEXT("No items were found on player %s"), *P->GetDisplayName()));
		C.Reply(FString::Printf(TEXT("Removed %d item(s) from player %s"), Removed, *P->GetDisplayName()));
		return true;
	}

	FMCState ParseBlockState(const FString& Tok)
	{
		FString Name = StripNs(Tok), Props;
		int32 Br;
		if (Name.FindChar('[', Br)) { Props = Name.Mid(Br); Name = Name.Left(Br); }
		const FMCBlock* B = FMCBlocks::Find(FName(*Name));
		if (!B) return 0xFFFF;
		uint16 Meta = 0;
		if (Props.Contains(TEXT("axis=x"))) Meta = 1; else if (Props.Contains(TEXT("axis=z"))) Meta = 2;
		if (Props.Contains(TEXT("half=top")) || Props.Contains(TEXT("type=top"))) Meta = 1;
		if (Props.Contains(TEXT("type=double"))) Meta = 2;
		int32 Lev;
		if (Props.Contains(TEXT("level=")) && ParseInt(Props.Mid(Props.Find(TEXT("level=")) + 6, 2).Replace(TEXT("]"), TEXT("")), Lev)) Meta = (uint16)Lev;
		return B->State(Meta);
	}

	bool CmdSetblock(FMCCommandContext& C)
	{
		FMCBlockPos P;
		if (C.Args.Num() < 5 || !C.ParseBlockPos(1, P)) return C.Fail(TEXT("Usage: /setblock <x y z> <block> [destroy|keep|replace]"));
		const FMCState S = ParseBlockState(C.Args[4]);
		if (S == 0xFFFF) return C.Fail(FString::Printf(TEXT("Unknown block type '%s'"), *C.Args[4]));
		if (!C.World->IsReadyAt(P) || !FMCChunk::InRange(P.Z)) return C.Fail(TEXT("That position is not loaded"));
		const FString Mode = C.Args.IsValidIndex(5) ? C.Args[5].ToLower() : TEXT("replace");
		if (Mode == TEXT("keep") && !C.World->IsAir(P)) return C.Fail(TEXT("Could not set the block"));
		if (Mode == TEXT("destroy")) C.World->DestroyBlock(P, true);
		if (C.World->GetState(P) == S) return C.Fail(TEXT("Could not set the block"));
		C.World->SetState(P, S, MCSet_Default);
		C.Reply(FString::Printf(TEXT("Changed the block at %s"), *BlockText(P)));
		return true;
	}

	bool CmdFill(FMCCommandContext& C)
	{
		FMCBlockPos A, B;
		if (C.Args.Num() < 8 || !C.ParseBlockPos(1, A) || !C.ParseBlockPos(4, B)) return C.Fail(TEXT("Usage: /fill <from> <to> <block> [replace|destroy|keep|hollow|outline]"));
		const FMCState S = ParseBlockState(C.Args[7]);
		if (S == 0xFFFF) return C.Fail(FString::Printf(TEXT("Unknown block type '%s'"), *C.Args[7]));
		const FMCBlockPos Lo(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y), FMath::Max(FMath::Min(A.Z, B.Z), MC::MinZ));
		const FMCBlockPos Hi(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y), FMath::Min(FMath::Max(A.Z, B.Z), MC::MaxZ));
		const int64 Volume = int64(Hi.X - Lo.X + 1) * (Hi.Y - Lo.Y + 1) * (Hi.Z - Lo.Z + 1);
		if (Volume > 32768) return C.Fail(FString::Printf(TEXT("Too many blocks in the specified area (maximum 32768, specified %lld)"), Volume));
		const FString Mode = C.Args.IsValidIndex(8) ? C.Args[8].ToLower() : TEXT("replace");
		FMCState Filter = 0xFFFF;
		if (Mode == TEXT("replace") && C.Args.IsValidIndex(9)) Filter = ParseBlockState(C.Args[9]);
		int32 Changed = 0;
		for (int32 Z = Lo.Z; Z <= Hi.Z; ++Z)
			for (int32 Y = Lo.Y; Y <= Hi.Y; ++Y)
				for (int32 X = Lo.X; X <= Hi.X; ++X)
				{
					const FMCBlockPos P(X, Y, Z);
					if (!C.World->IsReadyAt(P)) continue;
					const bool bEdge = X == Lo.X || X == Hi.X || Y == Lo.Y || Y == Hi.Y || Z == Lo.Z || Z == Hi.Z;
					FMCState Target = S;
					if ((Mode == TEXT("hollow")) && !bEdge) Target = 0;
					if (Mode == TEXT("outline") && !bEdge) continue;
					const FMCState Cur = C.World->GetState(P);
					if (Mode == TEXT("keep") && Cur != 0) continue;
					if (Filter != 0xFFFF && FMCBlocks::BlockOf(Cur) != FMCBlocks::BlockOf(Filter)) continue;
					if (Cur == Target) continue;
					if (Mode == TEXT("destroy")) C.World->DestroyBlock(P, true);
					if (C.World->SetState(P, Target, MCSet_Render | MCSet_Light | MCSet_Neighbors)) ++Changed;
				}
		if (Changed == 0) return C.Fail(TEXT("No blocks were filled"));
		C.Reply(FString::Printf(TEXT("Successfully filled %d block(s)"), Changed));
		return true;
	}

	bool* RuleBool(FMCGameRules& R, const FString& N)
	{
		const FString K = NormRule(N);
		if (K == TEXT("dodaylightcycle") || K == TEXT("advancetime")) return &R.bDoDaylightCycle;
		if (K == TEXT("doweathercycle") || K == TEXT("advanceweather")) return &R.bDoWeatherCycle;
		if (K == TEXT("domobspawning") || K == TEXT("spawnmobs")) return &R.bDoMobSpawning;
		if (K == TEXT("keepinventory")) return &R.bKeepInventory;
		if (K == TEXT("mobgriefing")) return &R.bMobGriefing;
		if (K == TEXT("dofiretick") || K == TEXT("firespreadradiusaroundplayer")) return &R.bDoFireTick;
		if (K == TEXT("naturalregeneration") || K == TEXT("naturalhealthregeneration")) return &R.bNaturalRegeneration;
		if (K == TEXT("showcoordinates") || K == TEXT("reduceddebuginfo")) return &R.bShowCoordinates;
		if (K == TEXT("doimmediaterespawn") || K == TEXT("immediaterespawn")) return &R.bDoImmediateRespawn;
		if (K == TEXT("falldamage")) return &R.bFallDamage;
		if (K == TEXT("drowningdamage")) return &R.bDrowningDamage;
		if (K == TEXT("firedamage")) return &R.bFireDamage;
		if (K == TEXT("dotiledrops") || K == TEXT("blockdrops")) return &R.bDoTileDrops;
		if (K == TEXT("domobloot") || K == TEXT("mobdrops")) return &R.bDoMobLoot;
		return nullptr;
	}

	bool CmdGamerule(FMCCommandContext& C)
	{
		if (C.Args.Num() < 2) return C.Fail(TEXT("Usage: /gamerule <rule> [value]"));
		FMCGameRules& R = C.Game->Rules;
		const FString Rule = StripNs(C.Args[1]);
		const FString K = NormRule(Rule);
		if (K == TEXT("randomtickspeed") || K == TEXT("spawnradius") || K == TEXT("respawnradius"))
		{
			int32& V = K == TEXT("randomtickspeed") ? R.RandomTickSpeed : R.SpawnRadius;
			if (!C.Args.IsValidIndex(2)) { C.Reply(FString::Printf(TEXT("Gamerule %s is currently set to: %d"), *Rule, V)); return true; }
			if (!ParseInt(C.Args[2], V)) return C.Fail(TEXT("Invalid integer"));
			if (K == TEXT("randomtickspeed")) for (int32 d = 0; d < 3; ++d) if (FMCWorld* W = C.Game->GetMCWorld((EMCDimension)d)) W->RandomTickSpeed = V;
			C.Reply(FString::Printf(TEXT("Gamerule %s is now set to: %d"), *Rule, V));
			return true;
		}
		bool* B = RuleBool(R, Rule);
		if (!B) return C.Fail(FString::Printf(TEXT("Unknown game rule '%s'"), *Rule));
		if (!C.Args.IsValidIndex(2)) { C.Reply(FString::Printf(TEXT("Gamerule %s is currently set to: %s"), *Rule, *B ? TEXT("true") : TEXT("false"))); return true; }
		const FString V = C.Args[2].ToLower();
		if (V != TEXT("true") && V != TEXT("false")) return C.Fail(TEXT("Expected true or false"));
		*B = V == TEXT("true");
		C.Reply(FString::Printf(TEXT("Gamerule %s is now set to: %s"), *Rule, *V));
		return true;
	}

	bool CmdHelp(FMCCommandContext& C)
	{
		C.Reply(TEXT("§eAvailable commands:"));
		FString Line;
		for (const TCHAR* Cmd : GCommands) { Line += TEXT("/"); Line += Cmd; Line += TEXT("  "); }
		C.Reply(Line);
		C.Reply(TEXT("Coordinates: x y z (y = height); ~ relative, ^ local. Selectors: @s @p @a @e[type=zombie,distance=..10,limit=3]"));
		return true;
	}
}

bool AMCGame::ExecuteCommand(const FString& CmdLine, AMCPlayer* Source)
{
	FString Line = CmdLine.TrimStartAndEnd();
	if (Line.StartsWith(TEXT("/"))) Line.RightChopInline(1);
	if (Line.IsEmpty()) return false;
	FMCCommandContext C;
	C.Game = this;
	C.Source = Source ? Source : Player.Get();
	C.World = C.Source && C.Source->World ? C.Source->World : ActiveWorld();
	C.Args = Tokenize(Line);
	if (C.Args.Num() == 0) return false;
	const FString Cmd = StripNs(C.Args[0]).ToLower();
	if (!bAllowCheats && Cmd != TEXT("help") && Cmd != TEXT("seed") && Cmd != TEXT("say")) return C.Fail(TEXT("You do not have permission to use this command (cheats are disabled)"));
	UE_LOG(LogOpus55, Log, TEXT("Command: /%s"), *Line);

	if (Cmd == TEXT("gamemode"))
	{
		EMCGameMode M;
		if (C.Args.Num() < 2 || !ParseMode(C.Args[1], M)) return C.Fail(TEXT("Usage: /gamemode <survival|creative|adventure|spectator> [target]"));
		AMCPlayer* P = C.ResolvePlayer(2);
		if (!P) return C.Fail(TEXT("No player was found"));
		P->SetGameMode(M);
		C.Reply(FString::Printf(TEXT("Set own game mode to %s"), *ModeName(M)));
		return true;
	}
	if (Cmd == TEXT("defaultgamemode"))
	{
		EMCGameMode M;
		if (C.Args.Num() < 2 || !ParseMode(C.Args[1], M)) return C.Fail(TEXT("Usage: /defaultgamemode <mode>"));
		DefaultGameMode = M;
		C.Reply(FString::Printf(TEXT("The default game mode is now %s"), *ModeName(M)));
		return true;
	}
	if (Cmd == TEXT("give")) return CmdGive(C);
	if (Cmd == TEXT("summon")) return CmdSummon(C);
	if (Cmd == TEXT("time")) return CmdTime(C);
	if (Cmd == TEXT("weather")) return CmdWeather(C);
	if (Cmd == TEXT("tp") || Cmd == TEXT("teleport")) return CmdTeleport(C);
	if (Cmd == TEXT("locate")) return CmdLocate(C);
	if (Cmd == TEXT("kill")) return CmdKill(C);
	if (Cmd == TEXT("effect")) return CmdEffect(C);
	if (Cmd == TEXT("enchant")) return CmdEnchant(C);
	if (Cmd == TEXT("xp") || Cmd == TEXT("experience")) return CmdXP(C);
	if (Cmd == TEXT("clear")) return CmdClear(C);
	if (Cmd == TEXT("setblock")) return CmdSetblock(C);
	if (Cmd == TEXT("fill")) return CmdFill(C);
	if (Cmd == TEXT("gamerule")) return CmdGamerule(C);
	if (Cmd == TEXT("help") || Cmd == TEXT("?")) return CmdHelp(C);
	if (Cmd == TEXT("difficulty"))
	{
		static const TCHAR* Names[] = { TEXT("Peaceful"), TEXT("Easy"), TEXT("Normal"), TEXT("Hard") };
		if (C.Args.Num() < 2) { C.Reply(FString::Printf(TEXT("The difficulty is %s"), Names[(int32)Difficulty])); return true; }
		const FString D = C.Args[1].ToLower();
		EMCDifficulty New;
		if (D == TEXT("peaceful") || D == TEXT("p") || D == TEXT("0")) New = EMCDifficulty::Peaceful;
		else if (D == TEXT("easy") || D == TEXT("e") || D == TEXT("1")) New = EMCDifficulty::Easy;
		else if (D == TEXT("normal") || D == TEXT("n") || D == TEXT("2")) New = EMCDifficulty::Normal;
		else if (D == TEXT("hard") || D == TEXT("h") || D == TEXT("3")) New = EMCDifficulty::Hard;
		else return C.Fail(FString::Printf(TEXT("Invalid difficulty '%s'"), *D));
		if (bHardcore) return C.Fail(TEXT("Difficulty is locked in Hardcore mode"));
		Difficulty = New;
		if (New == EMCDifficulty::Peaceful)
			if (FMCWorld* W = ActiveWorld())
				for (AMCEntity* E : W->Entities) if (AMCMob* M = Cast<AMCMob>(E)) if (M->Def && M->Def->Category == EMCMobCategory::Monster && !M->bPersistent) M->Discard();
		C.Reply(FString::Printf(TEXT("The difficulty has been set to %s"), Names[(int32)New]));
		return true;
	}
	if (Cmd == TEXT("seed")) { C.Reply(FString::Printf(TEXT("Seed: [%lld]"), (int64)Seed)); return true; }
	if (Cmd == TEXT("say")) { C.Reply(TEXT("[Steve] ") + Line.Mid(4)); return true; }
	if (Cmd == TEXT("save-all")) { SaveWorld(true); C.Reply(TEXT("Saved the game")); return true; }
	if (Cmd == TEXT("setworldspawn"))
	{
		FMCBlockPos P = C.Source ? C.Source->BlockPos() : WorldSpawn;
		if (C.Args.Num() >= 4 && !C.ParseBlockPos(1, P)) return C.Fail(TEXT("Invalid position"));
		WorldSpawn = P;
		C.Reply(FString::Printf(TEXT("Set the world spawn point to %s"), *BlockText(P)));
		return true;
	}
	if (Cmd == TEXT("spawnpoint"))
	{
		AMCPlayer* P = C.ResolvePlayer(1);
		if (!P) return C.Fail(TEXT("No player was found"));
		FMCBlockPos Pos = P->BlockPos();
		if (C.Args.Num() >= 5 && !C.ParseBlockPos(2, Pos)) return C.Fail(TEXT("Invalid position"));
		P->SetSpawnPoint(Pos, P->World ? P->World->Dim : EMCDimension::Overworld, true);
		C.Reply(FString::Printf(TEXT("Set spawn point to %s for %s"), *BlockText(Pos), *P->GetDisplayName()));
		return true;
	}
	if (Cmd == TEXT("particle"))
	{
		if (C.Args.Num() < 2) return C.Fail(TEXT("Usage: /particle <name> [x y z] [count]"));
		FVector P = C.Source ? C.Source->GetEyePos() + C.Source->GetLookDir() * 2.0 : C.Origin();
		if (C.Args.Num() >= 5 && !C.ParsePos(2, P, false)) return C.Fail(TEXT("Invalid position"));
		int32 Count = 12;
		if (C.Args.IsValidIndex(5)) ParseInt(C.Args[5], Count);
		SpawnParticles(C.World, FName(*StripNs(C.Args[1])), P, FMath::Clamp(Count, 1, 500), 0.5f, FVector::ZeroVector, FColor::White);
		C.Reply(FString::Printf(TEXT("Displaying particle %s"), *StripNs(C.Args[1])));
		return true;
	}
	return C.Fail(FString::Printf(TEXT("Unknown or incomplete command: %s. Type /help for a list"), *Cmd));
}

TArray<FString> AMCGame::GetCommandSuggestions(const FString& Partial) const
{
	TArray<FString> Out;
	FString Line = Partial;
	if (Line.StartsWith(TEXT("/"))) Line.RightChopInline(1);
	TArray<FString> Tok = Tokenize(Line);
	const bool bNewToken = Line.IsEmpty() || FChar::IsWhitespace(Line[Line.Len() - 1]);
	if (bNewToken) Tok.Add(FString());
	if (Tok.Num() == 0) Tok.Add(FString());
	const int32 Index = Tok.Num() - 1;
	const FString Prefix = StripNs(Tok[Index]).ToLower();
	FString Head = TEXT("/");
	for (int32 i = 0; i < Index; ++i) { Head += Tok[i]; Head += TEXT(" "); }

	TArray<FString> Options;
	const FString Cmd = Index > 0 ? StripNs(Tok[0]).ToLower() : FString();
	auto AddItems = [&]() { for (const FMCItem& I : FMCItems::All()) if (I.Id != 0) Options.Add(I.Name.ToString()); };
	auto AddBlocks = [&]() { for (const FMCBlock& B : FMCBlocks::All()) Options.Add(B.Name.ToString()); };
	auto AddTargets = [&]() { Options.Append({ TEXT("@s"), TEXT("@p"), TEXT("@a"), TEXT("@e"), TEXT("@r"), TEXT("Steve") }); };
	auto AddCoords = [&]() { Options.Append({ TEXT("~"), TEXT("~ ~ ~"), TEXT("^ ^ ^") }); };
	if (Index == 0) for (const TCHAR* C : GCommands) Options.Add(C);
	else if (Cmd == TEXT("gamemode") || Cmd == TEXT("defaultgamemode")) { if (Index == 1) Options.Append({ TEXT("survival"), TEXT("creative"), TEXT("adventure"), TEXT("spectator") }); else if (Index == 2) AddTargets(); }
	else if (Cmd == TEXT("give")) { if (Index == 1) AddTargets(); else if (Index == 2) AddItems(); else if (Index == 3) Options.Append({ TEXT("1"), TEXT("16"), TEXT("64") }); }
	else if (Cmd == TEXT("summon"))
	{
		if (Index == 1) { for (const FMCMobDef& D : MCMobs::All()) Options.Add(D.Id.ToString()); for (const TCHAR* E : GOtherEntities) Options.Add(E); }
		else if (Index <= 4) AddCoords();
	}
	else if (Cmd == TEXT("time")) { if (Index == 1) Options.Append({ TEXT("set"), TEXT("add"), TEXT("query") }); else if (Index == 2) Options.Append({ TEXT("day"), TEXT("noon"), TEXT("night"), TEXT("midnight"), TEXT("daytime"), TEXT("gametime") }); }
	else if (Cmd == TEXT("weather")) { if (Index == 1) Options.Append({ TEXT("clear"), TEXT("rain"), TEXT("thunder") }); }
	else if (Cmd == TEXT("difficulty")) { if (Index == 1) Options.Append({ TEXT("peaceful"), TEXT("easy"), TEXT("normal"), TEXT("hard") }); }
	else if (Cmd == TEXT("tp") || Cmd == TEXT("teleport") || Cmd == TEXT("kill") || Cmd == TEXT("clear") || Cmd == TEXT("spawnpoint")) { if (Index == 1) { AddTargets(); if (Cmd != TEXT("kill")) AddCoords(); } else if (Cmd == TEXT("clear") && Index == 2) AddItems(); else AddCoords(); }
	else if (Cmd == TEXT("locate"))
	{
		if (Index == 1) Options.Append({ TEXT("structure"), TEXT("biome") });
		else if (Index == 2)
		{
			const FString K = Tok[1].ToLower();
			if (K == TEXT("biome")) { for (int32 b = 0; b < FMCBiomes::Num(); ++b) Options.Add(FMCBiomes::Get((uint8)b).Name.ToString()); }
			else if (const FMCWorld* W = Player && Player->World ? Player->World : ActiveWorld()) if (W->Generator) { TArray<FName> N; W->Generator->GetStructureNames(N); for (FName S : N) Options.Add(S.ToString()); }
		}
	}
	else if (Cmd == TEXT("effect"))
	{
		if (Index == 1) Options.Append({ TEXT("give"), TEXT("clear") });
		else if (Index == 2) AddTargets();
		else if (Index == 3) for (int32 e = 1; e < (int32)EMCEffect::Count; ++e) Options.Add(MCEffects::Name((EMCEffect)e));
		else if (Index == 4) Options.Append({ TEXT("30"), TEXT("infinite") });
	}
	else if (Cmd == TEXT("enchant")) { if (Index == 1) AddTargets(); else if (Index == 2) for (int32 e = 1; e < (int32)EMCEnchant::Count; ++e) Options.Add(MCEnchants::Info((EMCEnchant)e).Id); }
	else if (Cmd == TEXT("xp") || Cmd == TEXT("experience")) { if (Index == 1) Options.Append({ TEXT("add"), TEXT("set"), TEXT("query") }); else if (Index == 2) AddTargets(); else if (Index == 4) Options.Append({ TEXT("points"), TEXT("levels") }); }
	else if (Cmd == TEXT("setblock")) { if (Index <= 3) AddCoords(); else if (Index == 4) AddBlocks(); else if (Index == 5) Options.Append({ TEXT("replace"), TEXT("destroy"), TEXT("keep") }); }
	else if (Cmd == TEXT("fill")) { if (Index <= 6) AddCoords(); else if (Index == 7 || Index == 9) AddBlocks(); else if (Index == 8) Options.Append({ TEXT("replace"), TEXT("destroy"), TEXT("keep"), TEXT("hollow"), TEXT("outline") }); }
	else if (Cmd == TEXT("gamerule")) { if (Index == 1) for (const TCHAR* R : GGameRules) Options.Add(R); else if (Index == 2) Options.Append({ TEXT("true"), TEXT("false") }); }
	else if (Cmd == TEXT("particle")) { if (Index == 1) Options.Append({ TEXT("flame"), TEXT("smoke"), TEXT("heart"), TEXT("portal"), TEXT("explosion"), TEXT("crit"), TEXT("enchant"), TEXT("happy_villager"), TEXT("angry_villager"), TEXT("cloud"), TEXT("end_rod"), TEXT("soul_fire_flame"), TEXT("dragon_breath"), TEXT("note"), TEXT("snowflake"), TEXT("totem_of_undying") }); }

	Options.Sort();
	for (const FString& O : Options)
	{
		if (!Prefix.IsEmpty() && !O.ToLower().StartsWith(Prefix) && !(Prefix.Len() >= 3 && O.ToLower().Contains(Prefix))) continue;
		Out.AddUnique(Head + O);
		if (Out.Num() >= 60) break;
	}
	return Out;
}
