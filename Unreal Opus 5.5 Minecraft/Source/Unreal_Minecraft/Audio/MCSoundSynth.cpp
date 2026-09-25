// Procedural sound synthesis: every sound id in the game is generated from scratch here (pads, noise bursts,
// FM hits, vowels, metal rings...). Deterministic per (id, variant) so results are reproducible.
#include "Audio/MCAudio.h"
#include "Core/MCCore.h"
#include "HAL/PlatformTime.h"
#include "Blocks/MCBlocks.h"

namespace
{
	// ------------------------------------------------------------------ infrastructure
	struct FSyn
	{
		FMCSoundClip& Clip;
		FMCRandom R;
		float Vol = 1.f;
		float T0 = 0.f;

		explicit FSyn(FMCSoundClip& In) : Clip(In) {}

		FORCEINLINE float Time(int32 i) const { return i / (float)Clip.SampleRate; }
		void Add(int32 Index, float V)
		{
			if (Index < 0 || Index >= Clip.Samples.Num()) return;
			const float Cur = Clip.Samples[Index] / 32767.f;
			Clip.Samples[Index] = (int16)FMath::Clamp((Cur + V) * 32767.f, -32768.f, 32767.f);
		}
		void AddAt(float Seconds, float V) { Add((int32)(Seconds * Clip.SampleRate), V); }
	};

	/** Simple one pole low pass. */
	struct FLP { float A = 0.f, Y = 0.f; float Run(float X, float CutHz, int32 SR) { const float Dt = 1.f / SR; A += (X - Y) * FMath::Clamp(2.f * PI * CutHz * Dt, 0.f, 1.f); Y = A; return Y; } };
	struct FHP { float Prev = 0.f, Y = 0.f; float Run(float X, float CutHz, int32 SR) { const float Dt = 1.f / SR; const float A = FMath::Exp(-2.f * PI * CutHz * Dt); Y = A * (Y + X - Prev); Prev = X; return Y; } };

	enum class FWave { Sine, Triangle, Saw, Square, Noise };

	FORCEINLINE float Wave(FWave W, float Phase)
	{
		Phase -= FMath::FloorToFloat(Phase);
		switch (W)
		{
		case FWave::Sine: return FMath::Sin(Phase * 2.f * PI);
		case FWave::Triangle: return 4.f * FMath::Abs(Phase - 0.5f) - 1.f;
		case FWave::Saw: return Phase * 2.f - 1.f;
		case FWave::Square: return Phase < 0.5f ? 1.f : -1.f;
		default: return 0.f;
		}
	}

	/** Pitched tone with exponential decay and pitch sweep. Writes additively into the clip. */
	void Tone(FSyn& S, float Start, float Dur, float Freq, float FreqEnd, FWave W, float Amp, float Decay, float Attack = 0.005f, float Vibrato = 0.f, bool bLoop = false)
	{
		const int32 SR = S.Clip.SampleRate;
		const int32 I0 = (int32)(Start * SR);
		const int32 N = (int32)(Dur * SR);
		float Phase = 0.f;
		for (int32 i = 0; i < N; ++i)
		{
			const float T = i / (float)SR;
			const float P = T / FMath::Max(0.001f, Dur);
			const float F = FMath::Lerp(Freq, FreqEnd, P) * (1.f + Vibrato * FMath::Sin(T * 2.f * PI * 5.5f));
			Phase += F / SR;
			const float Env = (Attack > 0.f ? FMath::Min(1.f, T / Attack) : 1.f) * FMath::Exp(-T * Decay);
			float V = Wave(W, Phase) * Env * Amp;
			if (bLoop) V *= 1.f;
			S.Add(I0 + i, V);
		}
	}

	/** Filtered noise burst (footsteps, wind, explosions, water). */
	void Noise(FSyn& S, float Start, float Dur, float Amp, float CutFrom, float CutTo, float Decay, bool bHighPass = false)
	{
		const int32 SR = S.Clip.SampleRate;
		const int32 I0 = (int32)(Start * SR);
		const int32 N = (int32)(Dur * SR);
		FLP LP; FHP HP;
		for (int32 i = 0; i < N; ++i)
		{
			const float T = i / (float)SR;
			const float P = T / FMath::Max(0.001f, Dur);
			const float Cut = FMath::Lerp(CutFrom, CutTo, P);
			float V = (S.R.NextFloat() * 2.f - 1.f);
			V = bHighPass ? HP.Run(V, Cut, SR) : LP.Run(V, Cut, SR);
			V *= FMath::Exp(-T * Decay) * Amp;
			S.Add(I0 + i, V);
		}
	}

	/** Plucked string / FM metal hit. */
	void FM(FSyn& S, float Start, float Dur, float Carrier, float Ratio, float Index, float Amp, float Decay, float Bright = 1.f)
	{
		const int32 SR = S.Clip.SampleRate;
		const int32 I0 = (int32)(Start * SR);
		const int32 N = (int32)(Dur * SR);
		for (int32 i = 0; i < N; ++i)
		{
			const float T = i / (float)SR;
			const float Env = FMath::Exp(-T * Decay);
			const float M = FMath::Sin(2.f * PI * Carrier * Ratio * T) * Index * Env * Bright;
			const float V = FMath::Sin(2.f * PI * Carrier * T + M) * Env * Amp;
			S.Add(I0 + i, V);
		}
	}

	/** Animal / creature voice: two formants on a buzzy source. */
	void Voice(FSyn& S, float Start, float Dur, float F0Start, float F0End, float F1, float F2, float Amp, float Growl = 0.f)
	{
		const int32 SR = S.Clip.SampleRate;
		const int32 I0 = (int32)(Start * SR);
		const int32 N = (int32)(Dur * SR);
		float Phase = 0.f;
		FLP L1, L2, L3;
		for (int32 i = 0; i < N; ++i)
		{
			const float T = i / (float)SR;
			const float P = T / FMath::Max(0.001f, Dur);
			float F0 = FMath::Lerp(F0Start, F0End, P);
			F0 *= 1.f + Growl * FMath::Sin(T * 2.f * PI * 24.f);
			Phase += F0 / SR;
			const float Src = Wave(FWave::Saw, Phase) * 0.6f + (S.R.NextFloat() * 2.f - 1.f) * (0.25f + Growl * 0.5f);
			const float V1 = L1.Run(Src, F1, SR);
			const float V2 = L2.Run(Src, F2, SR);
			const float V3 = L3.Run(Src, F0 * 3.f, SR);
			const float Env = FMath::Sin(FMath::Clamp(P, 0.f, 1.f) * PI) * 0.6f + FMath::Exp(-T * 3.f) * 0.4f;
			S.Add(I0 + i, (V1 * 0.5f + V2 * 0.3f + V3 * 0.2f) * Env * Amp);
		}
	}

	void Pad(FSyn& S, float Start, float Dur, const float* Freqs, int32 NumF, float Amp, float Attack, float Release, FWave W, float Detune)
	{
		const int32 SR = S.Clip.SampleRate;
		const int32 I0 = (int32)(Start * SR);
		const int32 N = (int32)(Dur * SR);
		TArray<float> Phase;
		Phase.SetNumZeroed(NumF * 2);
		for (int32 i = 0; i < N; ++i)
		{
			const float T = i / (float)SR;
			const float Env = FMath::Min(1.f, T / FMath::Max(0.001f, Attack)) * FMath::Min(1.f, (Dur - T) / FMath::Max(0.001f, Release));
			float Acc = 0.f;
			for (int32 k = 0; k < NumF; ++k)
			{
				Phase[k * 2] += (Freqs[k] * (1.f + Detune)) / SR;
				Phase[k * 2 + 1] += (Freqs[k] * (1.f - Detune)) / SR;
				Acc += Wave(W, Phase[k * 2]) * 0.5f + Wave(W, Phase[k * 2 + 1]) * 0.5f;
			}
			const float Trem = 0.85f + 0.15f * FMath::Sin(T * 2.f * PI * 0.25f + Freqs[0]);
			S.Add(I0 + i, Acc / NumF * Env * Amp * Trem);
		}
	}

	void Bell(FSyn& S, float Start, float Base, float Amp, float Decay)
	{
		FM(S, Start, FMath::Min(2.5f, 4.f / Decay), Base, 3.51f, 4.f, Amp, Decay);
		FM(S, Start, FMath::Min(2.5f, 4.f / (Decay * 0.6f)), Base, 2.f, 2.f, Amp * 0.5f, Decay * 0.6f);
	}

	void Splash(FSyn& S, float Start, float Amp, float Dur)
	{
		Noise(S, Start, Dur, Amp, 2600.f, 500.f, 6.f);
		Noise(S, Start + 0.02f, Dur * 1.4f, Amp * 0.5f, 700.f, 220.f, 3.f);
	}

	void Explode(FSyn& S, float Amp, float Power)
	{
		Noise(S, 0.f, 1.4f + Power * 0.12f, Amp, 900.f, 60.f, 2.4f);
		Noise(S, 0.f, 0.22f, Amp * 1.2f, 5000.f, 400.f, 12.f, true);
		Tone(S, 0.f, 0.6f, 70.f, 28.f, FWave::Sine, Amp * 0.9f, 5.f, 0.002f);
		Noise(S, 0.05f, 2.2f, Amp * 0.35f, 320.f, 90.f, 1.4f);
	}

	// ------------------------------------------------------------------ ids
	FString Base(const FString& N)
	{
		if (N.StartsWith(TEXT("block_")))
		{
			// block_<group>_<kind>
			int32 Last = INDEX_NONE;
			N.FindLastChar(TCHAR('_'), Last);
			if (Last != INDEX_NONE) return N.Left(Last);
		}
		if (N.EndsWith(TEXT("_ambient")) || N.EndsWith(TEXT("_hurt")) || N.EndsWith(TEXT("_death")) || N.EndsWith(TEXT("_step")) || N.EndsWith(TEXT("_attack"))
			|| N.EndsWith(TEXT("_shoot")) || N.EndsWith(TEXT("_idle")) || N.EndsWith(TEXT("_eat")) || N.EndsWith(TEXT("_say")))
		{
			int32 Last = INDEX_NONE;
			N.FindLastChar(TCHAR('_'), Last);
			return N.Left(Last);
		}
		return N;
	}

	const TCHAR* SoundGroupName(EMCSound S)
	{
		switch (S)
		{
		case EMCSound::Stone: return TEXT("stone");
		case EMCSound::Wood: return TEXT("wood");
		case EMCSound::Gravel: return TEXT("gravel");
		case EMCSound::Grass: return TEXT("grass");
		case EMCSound::Sand: return TEXT("sand");
		case EMCSound::Snow: return TEXT("snow");
		case EMCSound::Glass: return TEXT("glass");
		case EMCSound::Wool: return TEXT("cloth");
		case EMCSound::Metal: return TEXT("metal");
		case EMCSound::Slime: return TEXT("slime");
		case EMCSound::Honey: return TEXT("honey");
		case EMCSound::Netherrack: return TEXT("netherrack");
		case EMCSound::Nylium: return TEXT("nylium");
		case EMCSound::Bone: return TEXT("bone");
		case EMCSound::SoulSand: return TEXT("soul_sand");
		case EMCSound::Amethyst: return TEXT("amethyst");
		case EMCSound::Sculk: return TEXT("sculk");
		case EMCSound::Deepslate: return TEXT("deepslate");
		case EMCSound::Mud: return TEXT("mud");
		case EMCSound::Copper: return TEXT("copper");
		case EMCSound::Chain: return TEXT("chain");
		case EMCSound::Lantern: return TEXT("lantern");
		case EMCSound::Ladder: return TEXT("ladder");
		case EMCSound::Scaffolding: return TEXT("scaffolding");
		case EMCSound::Crop: return TEXT("crop");
		case EMCSound::Coral: return TEXT("coral");
		case EMCSound::Wart: return TEXT("wart");
		case EMCSound::Fungus: return TEXT("fungus");
		case EMCSound::Basalt: return TEXT("basalt");
		case EMCSound::Cloth: return TEXT("cloth");
		case EMCSound::Candle: return TEXT("candle");
		case EMCSound::Moss: return TEXT("moss");
		case EMCSound::Froglight: return TEXT("froglight");
		case EMCSound::Lodestone: return TEXT("lodestone");
		case EMCSound::Bamboo: return TEXT("bamboo");
		case EMCSound::Cherry: return TEXT("cherry");
		default: return TEXT("stone");
		}
	}

	/** Recipes per block material group: base cut-off, decay, resonance, metallic ring. */
	struct FBlockSound { float Cut; float Decay; float Ring; float RingHz; bool bSoft; };
	FBlockSound BlockSound(const FString& Group)
	{
		if (Group == TEXT("block_stone") || Group == TEXT("block_deepslate") || Group == TEXT("block_basalt") || Group == TEXT("block_lodestone")) return { 1400.f, 9.f, 0.08f, 220.f, false };
		if (Group == TEXT("block_wood")) return { 900.f, 12.f, 0.05f, 160.f, false };
		if (Group == TEXT("block_gravel") || Group == TEXT("block_mud") || Group == TEXT("block_soul_sand")) return { 1800.f, 7.f, 0.f, 0.f, true };
		if (Group == TEXT("block_grass") || Group == TEXT("block_moss") || Group == TEXT("block_wart") || Group == TEXT("block_fungus")) return { 2400.f, 11.f, 0.f, 0.f, true };
		if (Group == TEXT("block_sand")) return { 3000.f, 14.f, 0.f, 0.f, true };
		if (Group == TEXT("block_snow")) return { 1500.f, 12.f, 0.f, 0.f, true };
		if (Group == TEXT("block_glass")) return { 6000.f, 16.f, 0.35f, 2400.f, false };
		if (Group == TEXT("block_cloth") || Group == TEXT("block_wool")) return { 600.f, 14.f, 0.f, 0.f, true };
		if (Group == TEXT("block_metal") || Group == TEXT("block_copper")) return { 4200.f, 8.f, 0.5f, 780.f, false };
		if (Group == TEXT("block_chain") || Group == TEXT("block_lantern")) return { 5200.f, 7.f, 0.55f, 1400.f, false };
		if (Group == TEXT("block_slime")) return { 700.f, 5.f, 0.1f, 130.f, true };
		if (Group == TEXT("block_honey")) return { 500.f, 4.f, 0.f, 0.f, true };
		if (Group == TEXT("block_amethyst") || Group == TEXT("block_froglight")) return { 6500.f, 9.f, 0.45f, 1900.f, false };
		if (Group == TEXT("block_sculk")) return { 1200.f, 6.f, 0.3f, 90.f, true };
		if (Group == TEXT("block_netherrack") || Group == TEXT("block_nylium")) return { 2000.f, 8.f, 0.05f, 180.f, true };
		if (Group == TEXT("block_bone")) return { 3400.f, 12.f, 0.3f, 620.f, false };
		if (Group == TEXT("block_crop") || Group == TEXT("block_bamboo") || Group == TEXT("block_cherry")) return { 2600.f, 13.f, 0.05f, 300.f, true };
		if (Group == TEXT("block_coral")) return { 2800.f, 10.f, 0.15f, 700.f, true };
		return { 1600.f, 9.f, 0.1f, 240.f, false };
	}

	/** Recipes per mob family for ambient / hurt / death. */
	struct FMobSound { float F0; float F1; float F2; float Growl; float Dur; };
	FMobSound MobSound(const FString& Base1)
	{
		if (Base1 == TEXT("zombie") || Base1 == TEXT("husk") || Base1 == TEXT("drowned") || Base1 == TEXT("zombie_villager") || Base1 == TEXT("zombified_piglin")) return { 190.f, 620.f, 1150.f, 0.35f, 1.1f };
		if (Base1 == TEXT("skeleton") || Base1 == TEXT("stray") || Base1 == TEXT("bogged") || Base1 == TEXT("parched") || Base1 == TEXT("wither_skeleton")) return { 330.f, 1500.f, 2600.f, 0.05f, 0.7f };
		if (Base1 == TEXT("creeper")) return { 260.f, 1200.f, 3000.f, 0.1f, 0.6f };
		if (Base1 == TEXT("spider") || Base1 == TEXT("cave_spider")) return { 520.f, 2600.f, 4200.f, 0.2f, 0.5f };
		if (Base1 == TEXT("enderman")) return { 150.f, 500.f, 1100.f, 0.6f, 1.3f };
		if (Base1 == TEXT("cow") || Base1 == TEXT("mooshroom")) return { 150.f, 480.f, 900.f, 0.15f, 1.4f };
		if (Base1 == TEXT("pig")) return { 300.f, 800.f, 1600.f, 0.25f, 0.5f };
		if (Base1 == TEXT("sheep")) return { 420.f, 1300.f, 2400.f, 0.5f, 0.8f };
		if (Base1 == TEXT("chicken")) return { 900.f, 2200.f, 3600.f, 0.05f, 0.4f };
		if (Base1 == TEXT("wolf")) return { 220.f, 700.f, 1800.f, 0.3f, 0.6f };
		if (Base1 == TEXT("cat")) return { 480.f, 1100.f, 2400.f, 0.4f, 0.7f };
		if (Base1 == TEXT("horse") || Base1 == TEXT("donkey") || Base1 == TEXT("mule")) return { 240.f, 700.f, 1500.f, 0.3f, 1.0f };
		if (Base1 == TEXT("villager") || Base1 == TEXT("wandering_trader")) return { 170.f, 620.f, 1700.f, 0.1f, 0.6f };
		if (Base1 == TEXT("ghast")) return { 120.f, 400.f, 900.f, 0.7f, 1.8f };
		if (Base1 == TEXT("blaze")) return { 400.f, 2400.f, 5000.f, 0.1f, 0.7f };
		if (Base1 == TEXT("slime") || Base1 == TEXT("magma_cube")) return { 200.f, 900.f, 1800.f, 0.05f, 0.5f };
		if (Base1 == TEXT("bat")) return { 1800.f, 4200.f, 7000.f, 0.0f, 0.25f };
		if (Base1 == TEXT("parrot")) return { 1400.f, 3400.f, 6000.f, 0.05f, 0.3f };
		if (Base1 == TEXT("wither") || Base1 == TEXT("ender_dragon")) return { 90.f, 320.f, 700.f, 0.6f, 2.2f };
		if (Base1 == TEXT("guardian") || Base1 == TEXT("elder_guardian")) return { 260.f, 900.f, 2200.f, 0.35f, 0.9f };
		if (Base1 == TEXT("warden")) return { 70.f, 260.f, 620.f, 0.8f, 1.6f };
		if (Base1 == TEXT("bee")) return { 380.f, 1600.f, 3000.f, 0.1f, 0.4f };
		if (Base1 == TEXT("goat")) return { 320.f, 1000.f, 2000.f, 0.4f, 0.8f };
		if (Base1 == TEXT("frog")) return { 260.f, 1100.f, 1800.f, 0.6f, 0.5f };
		if (Base1 == TEXT("cod") || Base1 == TEXT("salmon") || Base1 == TEXT("tropical_fish")) return { 700.f, 2400.f, 3400.f, 0.0f, 0.2f };
		if (Base1 == TEXT("squid") || Base1 == TEXT("glow_squid")) return { 240.f, 700.f, 1500.f, 0.3f, 0.6f };
		if (Base1 == TEXT("dolphin")) return { 900.f, 2800.f, 5200.f, 0.0f, 0.3f };
		if (Base1 == TEXT("llama") || Base1 == TEXT("trader_llama")) return { 300.f, 900.f, 2100.f, 0.35f, 0.8f };
		if (Base1 == TEXT("panda")) return { 200.f, 600.f, 1300.f, 0.3f, 0.9f };
		if (Base1 == TEXT("polar_bear")) return { 160.f, 500.f, 1100.f, 0.45f, 1.2f };
		if (Base1 == TEXT("ravager")) return { 110.f, 380.f, 900.f, 0.6f, 1.5f };
		if (Base1 == TEXT("phantom")) return { 700.f, 2000.f, 4200.f, 0.2f, 0.7f };
		if (Base1 == TEXT("shulker")) return { 300.f, 1000.f, 2600.f, 0.3f, 0.8f };
		if (Base1 == TEXT("vex") || Base1 == TEXT("allay")) return { 1200.f, 3000.f, 5200.f, 0.05f, 0.4f };
		if (Base1 == TEXT("silverfish") || Base1 == TEXT("endermite")) return { 1600.f, 4200.f, 6800.f, 0.05f, 0.3f };
		if (Base1 == TEXT("fox")) return { 600.f, 1600.f, 3000.f, 0.3f, 0.5f };
		if (Base1 == TEXT("turtle")) return { 200.f, 700.f, 1400.f, 0.4f, 1.0f };
		if (Base1 == TEXT("rabbit")) return { 1100.f, 2600.f, 4400.f, 0.05f, 0.25f };
		if (Base1 == TEXT("strider")) return { 220.f, 800.f, 1700.f, 0.55f, 0.8f };
		if (Base1 == TEXT("hoglin") || Base1 == TEXT("zoglin")) return { 140.f, 500.f, 1200.f, 0.6f, 1.1f };
		if (Base1 == TEXT("piglin") || Base1 == TEXT("piglin_brute")) return { 200.f, 700.f, 1600.f, 0.4f, 0.7f };
		if (Base1 == TEXT("sniffer")) return { 120.f, 420.f, 900.f, 0.7f, 1.8f };
		if (Base1 == TEXT("camel") || Base1 == TEXT("camel_husk")) return { 180.f, 620.f, 1300.f, 0.5f, 1.2f };
		if (Base1 == TEXT("armadillo")) return { 380.f, 1100.f, 2100.f, 0.2f, 0.6f };
		if (Base1 == TEXT("breeze")) return { 500.f, 1800.f, 3800.f, 0.2f, 0.6f };
		return { 300.f, 900.f, 1800.f, 0.3f, 0.6f };
	}

	int32 HashName(const FString& S) { return (int32)(MCHash::StringHash(*S) & 0x7FFFFFFF); }
}

namespace MCSoundSynth
{
	int32 NumVariants(FName Sound)
	{
		const FString N = Sound.ToString();
		if (N.StartsWith(TEXT("block_")) && (N.EndsWith(TEXT("_step")) || N.EndsWith(TEXT("_break")) || N.EndsWith(TEXT("_place")) || N.EndsWith(TEXT("_hit")))) return 4;
		if (N.EndsWith(TEXT("_ambient")) || N.EndsWith(TEXT("_hurt")) || N.EndsWith(TEXT("_death"))) return 3;
		return 2;
	}

	void Generate(FName SoundId, int32 Variant, FMCSoundClip& Out)
	{
		const FString N = SoundId.ToString();
		const FString B = Base(N);
		const uint32 Seed = (uint32)(HashName(N) * 31 + Variant * 7919);
		FSyn S(Out);
		S.R.SetSeed(Seed);

		// ---------------- block sounds
		if (N.StartsWith(TEXT("block_")))
		{
			const bool bStep = N.EndsWith(TEXT("_step")), bBreak = N.EndsWith(TEXT("_break")), bPlace = N.EndsWith(TEXT("_place")), bHit = N.EndsWith(TEXT("_hit"));
			// recover the material group (block_<group>_<kind>); groups with underscores are matched by kind suffix
			FString Group = B;
			const FBlockSound BS = BlockSound(Group);
			Out.SampleRate = 32000;
			Out.Samples.SetNumZeroed((int32)((bStep ? 0.3f : (bBreak ? 0.6f : 0.5f)) * Out.SampleRate));
			const float Amp = (bStep ? 0.32f : (bBreak ? 0.85f : (bPlace ? 0.7f : 0.4f))) * S.R.FRange(0.85f, 1.15f);
			const int32 Layers = (BS.bSoft ? 1 : 2) + (bBreak ? 1 : 0);
			for (int32 L = 0; L < Layers; ++L)
			{
				const float Cut = BS.Cut * S.R.FRange(0.7f, 1.3f);
				Noise(S, L * 0.004f, bStep ? 0.16f : 0.3f, Amp / Layers, Cut, Cut * 0.35f, BS.Decay * S.R.FRange(0.8f, 1.2f), false);
			}
			if (BS.Ring > 0.02f)
			{
				const int32 Rings = bBreak ? 4 : 2;
				for (int32 k = 0; k < Rings; ++k)
					FM(S, k * 0.012f, 0.35f, BS.RingHz * (1.f + k * 0.37f) * S.R.FRange(0.94f, 1.06f), 1.4f + k * 0.2f, 2.5f, Amp * BS.Ring * (1.f - k * 0.2f), 22.f);
			}
			if (bHit) { Tone(S, 0.f, 0.12f, BS.RingHz, BS.RingHz * 0.8f, FWave::Triangle, Amp * 0.3f, 30.f); }
			return;
		}

		// ---------------- mob sounds
		if (N.EndsWith(TEXT("_ambient")) || N.EndsWith(TEXT("_hurt")) || N.EndsWith(TEXT("_death")) || N.EndsWith(TEXT("_attack")) || N.EndsWith(TEXT("_shoot")) || N.EndsWith(TEXT("_say")) || N.EndsWith(TEXT("_idle")))
		{
			const FMobSound MS = MobSound(B);
			const bool bHurt = N.EndsWith(TEXT("_hurt")), bDeath = N.EndsWith(TEXT("_death")), bAmbient = N.EndsWith(TEXT("_ambient"));
			const bool bShoot = N.EndsWith(TEXT("_shoot")), bAttack = N.EndsWith(TEXT("_attack"));
			float Dur = MS.Dur * (bAmbient ? S.R.FRange(0.8f, 1.3f) : (bDeath ? 1.5f : 0.7f));
			if (bShoot) Dur = 0.6f;
			if (bAttack) Dur = 0.35f;
			Out.SampleRate = 32000;
			Out.Samples.SetNumZeroed((int32)((Dur + 0.1f) * Out.SampleRate));
			const float F0 = MS.F0 * S.R.FRange(0.9f, 1.12f) * (bDeath ? 0.85f : 1.f);
			const float F1 = bHurt || bDeath ? F0 * 1.5f : F0 * 0.85f;
			const float Amp = (bAmbient ? 0.6f : (bHurt ? 0.85f : (bDeath ? 0.9f : 0.7f))) * S.R.FRange(0.85f, 1.1f);
			if (bShoot)
			{
				// projectile cast: whoosh plus a short voice burst
				Noise(S, 0.f, Dur, Amp * 0.5f, 2600.f, 700.f, 8.f);
				Voice(S, 0.f, Dur * 0.6f, F0 * 1.2f, F0 * 0.6f, MS.F1, MS.F2, Amp * 0.7f, MS.Growl);
			}
			else if (bAttack)
			{
				Voice(S, 0.f, Dur, F0 * 1.4f, F0 * 0.7f, MS.F1 * 1.2f, MS.F2, Amp, MS.Growl + 0.2f);
				Noise(S, 0.f, 0.12f, Amp * 0.4f, 3200.f, 900.f, 20.f);
			}
			else
			{
				Voice(S, 0.f, Dur, F0, F1, MS.F1, MS.F2, Amp, MS.Growl);
				if (bDeath) { Voice(S, Dur * 0.4f, Dur * 0.8f, F1 * 0.8f, F1 * 0.35f, MS.F1 * 0.8f, MS.F2 * 0.7f, Amp * 0.8f, MS.Growl + 0.3f); Noise(S, 0.f, Dur, Amp * 0.2f, 1200.f, 300.f, 5.f); }
				if (bHurt) Noise(S, 0.f, 0.16f, Amp * 0.35f, 3600.f, 1200.f, 22.f);
			}
			return;
		}

		// ---------------- named one shots
		Out.SampleRate = 32000;
		int32 Len = (int32)(0.4f * Out.SampleRate);
		auto Ensure = [&](float Sec) { if (Len < (int32)((Sec + 0.05f) * Out.SampleRate)) Len = (int32)((Sec + 0.05f) * Out.SampleRate); };
		Ensure(0.5f);

		struct FOnce { const TCHAR* Id; int32 Kind; float P0, P1, P2; };
		// Kind: 0 = UI click, 1 = metallic, 2 = soft, 3 = water, 4 = explosion-ish, 5 = magic, 6 = wood, 7 = cloth/armour
		static const FOnce Table[] = {
			{ TEXT("item_pickup"), 2, 900.f, 0.10f, 0.f },
			{ TEXT("entity_item_pickup"), 2, 950.f, 0.12f, 0.f },
			{ TEXT("experience_orb_pickup"), 5, 1600.f, 0.15f, 0.f },
			{ TEXT("item_break"), 0, 1400.f, 0.35f, 0.f },
			{ TEXT("armor_equip"), 7, 700.f, 0.32f, 0.f },
			{ TEXT("shield_block"), 6, 300.f, 0.22f, 0.f },
			{ TEXT("shield_break"), 4, 500.f, 0.5f, 0.f },
			{ TEXT("anvil_use"), 1, 500.f, 0.5f, 3.f },
			{ TEXT("anvil_land"), 1, 320.f, 0.6f, 2.f },
			{ TEXT("anvil_destroy"), 1, 260.f, 0.7f, 2.f },
			{ TEXT("bell_ring"), 1, 620.f, 0.9f, 4.f },
			{ TEXT("player_levelup"), 5, 880.f, 0.5f, 0.f },
			{ TEXT("player_burp"), 2, 220.f, 0.35f, 0.6f },
			{ TEXT("explode"), 4, 1.f, 0.f, 0.f },
			{ TEXT("splash"), 3, 1.f, 0.f, 0.f },
			{ TEXT("splash_potion_break"), 3, 1.f, 0.f, 0.f },
			{ TEXT("piston_extend"), 1, 260.f, 0.28f, 0.f },
			{ TEXT("piston_contract"), 1, 210.f, 0.26f, 0.f },
			{ TEXT("lever_click"), 0, 1200.f, 0.07f, 0.f },
			{ TEXT("repeater_click"), 0, 1500.f, 0.06f, 0.f },
			{ TEXT("comparator_click"), 0, 1600.f, 0.06f, 0.f },
			{ TEXT("dispenser_dispense"), 1, 400.f, 0.25f, 0.f },
			{ TEXT("dispenser_launch"), 1, 420.f, 0.25f, 0.f },
			{ TEXT("dispenser_fail"), 0, 200.f, 0.12f, 0.f },
			{ TEXT("door_open"), 6, 380.f, 0.45f, 0.f },
			{ TEXT("door_close"), 6, 300.f, 0.45f, 0.f },
			{ TEXT("chest_open"), 6, 240.f, 0.5f, 0.f },
			{ TEXT("chest_close"), 6, 200.f, 0.5f, 0.f },
			{ TEXT("ender_chest_open"), 5, 520.f, 0.7f, 0.f },
			{ TEXT("ender_chest_close"), 5, 480.f, 0.6f, 0.f },
			{ TEXT("shulker_box_open"), 2, 260.f, 0.5f, 0.f },
			{ TEXT("shulker_box_close"), 2, 240.f, 0.5f, 0.f },
			{ TEXT("barrel_open"), 6, 260.f, 0.4f, 0.f },
			{ TEXT("barrel_close"), 6, 230.f, 0.4f, 0.f },
			{ TEXT("furnace_crackle"), 4, 0.15f, 0.f, 0.f },
			{ TEXT("blast_furnace_crackle"), 4, 0.22f, 0.f, 0.f },
			{ TEXT("smoker_crackle"), 4, 0.18f, 0.f, 0.f },
			{ TEXT("campfire_crackle"), 4, 0.12f, 0.f, 0.f },
			{ TEXT("fire_extinguish"), 3, 0.5f, 0.f, 0.f },
			{ TEXT("lava_extinguish"), 3, 0.7f, 0.f, 0.f },
			{ TEXT("flint_and_steel_use"), 1, 1800.f, 0.2f, 0.f },
			{ TEXT("fire_charge_use"), 1, 1200.f, 0.25f, 0.f },
			{ TEXT("bucket_fill"), 3, 1.f, 0.f, 0.f },
			{ TEXT("bucket_empty"), 3, 1.f, 0.f, 0.f },
			{ TEXT("bucket_empty_lava"), 3, 1.2f, 0.f, 0.f },
			{ TEXT("bucket_fill_fish"), 3, 0.8f, 0.f, 0.f },
			{ TEXT("bottle_fill"), 3, 0.35f, 0.f, 0.f },
			{ TEXT("bottle_empty"), 3, 0.3f, 0.f, 0.f },
			{ TEXT("splash_potion_throw"), 2, 700.f, 0.3f, 0.f },
			{ TEXT("experience_bottle_throw"), 2, 800.f, 0.3f, 0.f },
			{ TEXT("ender_pearl_throw"), 2, 900.f, 0.3f, 0.f },
			{ TEXT("ender_pearl_teleport"), 5, 700.f, 0.5f, 0.f },
			{ TEXT("ender_eye_launch"), 5, 1000.f, 0.6f, 0.f },
			{ TEXT("ender_eye_death"), 5, 600.f, 0.4f, 0.f },
			{ TEXT("chorus_fruit_teleport"), 5, 900.f, 0.4f, 0.f },
			{ TEXT("dragon_egg_teleport"), 5, 400.f, 0.4f, 0.f },
			{ TEXT("bow_shoot"), 6, 1200.f, 0.3f, 0.f },
			{ TEXT("crossbow_shoot"), 6, 900.f, 0.3f, 0.f },
			{ TEXT("crossbow_loading_start"), 1, 600.f, 0.4f, 0.f },
			{ TEXT("crossbow_loading_middle"), 1, 700.f, 0.4f, 0.f },
			{ TEXT("crossbow_loading_end"), 1, 900.f, 0.4f, 0.f },
			{ TEXT("arrow_hit"), 0, 1500.f, 0.12f, 0.f },
			{ TEXT("arrow_hit_player"), 0, 900.f, 0.15f, 0.f },
			{ TEXT("trident_throw"), 1, 700.f, 0.35f, 0.f },
			{ TEXT("trident_return"), 1, 800.f, 0.4f, 0.f },
			{ TEXT("trident_hit"), 1, 600.f, 0.35f, 0.f },
			{ TEXT("trident_riptide"), 3, 1.f, 0.f, 0.f },
			{ TEXT("spear_lunge"), 1, 900.f, 0.3f, 0.f },
			{ TEXT("mace_smash"), 1, 200.f, 0.6f, 1.5f },
			{ TEXT("snowball_throw"), 2, 800.f, 0.25f, 0.f },
			{ TEXT("egg_throw"), 2, 1000.f, 0.25f, 0.f },
			{ TEXT("wind_charge_throw"), 2, 1200.f, 0.3f, 0.f },
			{ TEXT("wind_charge_burst"), 4, 0.6f, 0.f, 0.f },
			{ TEXT("firework_rocket_launch"), 4, 0.5f, 0.f, 0.f },
			{ TEXT("firework_rocket_twinkle"), 5, 2000.f, 0.6f, 0.f },
			{ TEXT("tnt_primed"), 6, 500.f, 0.6f, 0.f },
			{ TEXT("creeper_primed"), 2, 400.f, 0.9f, 0.f },
			{ TEXT("wither_spawn"), 4, 1.f, 0.f, 0.f },
			{ TEXT("wither_shoot"), 5, 300.f, 0.5f, 0.f },
			{ TEXT("wither_break_block"), 6, 260.f, 0.5f, 0.f },
			{ TEXT("blaze_shoot"), 5, 500.f, 0.35f, 0.f },
			{ TEXT("blaze_burn"), 4, 0.2f, 0.f, 0.f },
			{ TEXT("ghast_shoot"), 5, 260.f, 0.7f, 0.f },
			{ TEXT("ghast_warn"), 5, 220.f, 0.9f, 0.f },
			{ TEXT("shulker_shoot"), 5, 420.f, 0.4f, 0.f },
			{ TEXT("shulker_bullet_hit"), 1, 900.f, 0.25f, 0.f },
			{ TEXT("llama_spit"), 2, 700.f, 0.25f, 0.f },
			{ TEXT("witch_throw"), 2, 800.f, 0.3f, 0.f },
			{ TEXT("witch_drink"), 3, 0.6f, 0.f, 0.f },
			{ TEXT("warden_sonic_boom"), 4, 1.4f, 0.f, 0.f },
			{ TEXT("warden_dig"), 4, 0.7f, 0.f, 0.f },
			{ TEXT("warden_heartbeat"), 4, 0.3f, 0.f, 0.f },
			{ TEXT("breeze_shoot"), 2, 1400.f, 0.3f, 0.f },
			{ TEXT("breeze_jump"), 2, 900.f, 0.3f, 0.f },
			{ TEXT("guardian_attack"), 5, 600.f, 0.4f, 0.f },
			{ TEXT("elder_guardian_curse"), 5, 200.f, 1.2f, 0.f },
			{ TEXT("phantom_bite"), 6, 700.f, 0.25f, 0.f },
			{ TEXT("phantom_swoop"), 2, 900.f, 0.4f, 0.f },
			{ TEXT("ravager_roar"), 5, 160.f, 1.2f, 0.f },
			{ TEXT("enderman_stare"), 5, 300.f, 0.8f, 0.f },
			{ TEXT("enderman_scream"), 5, 260.f, 1.0f, 0.f },
			{ TEXT("ender_dragon_flap"), 4, 0.5f, 0.f, 0.f },
			{ TEXT("ender_dragon_growl"), 5, 120.f, 1.4f, 0.f },
			{ TEXT("ender_dragon_shoot"), 5, 180.f, 0.9f, 0.f },
			{ TEXT("dragon_fireball_explode"), 4, 1.2f, 0.f, 0.f },
			{ TEXT("dragon_breath"), 5, 400.f, 0.8f, 0.f },
			{ TEXT("portal_trigger"), 5, 300.f, 1.2f, 0.f },
			{ TEXT("portal_ignite"), 5, 250.f, 1.0f, 0.f },
			{ TEXT("portal_travel"), 5, 200.f, 1.4f, 0.f },
			{ TEXT("end_portal_spawn"), 5, 180.f, 1.6f, 0.f },
			{ TEXT("end_portal_frame_fill"), 5, 700.f, 0.6f, 0.f },
			{ TEXT("end_gateway_spawn"), 5, 260.f, 1.2f, 0.f },
			{ TEXT("end_gateway_travel"), 5, 400.f, 0.9f, 0.f },
			{ TEXT("respawn_anchor_set_spawn"), 5, 300.f, 1.0f, 0.f },
			{ TEXT("respawn_anchor_deplete"), 4, 0.9f, 0.f, 0.f },
			{ TEXT("respawn_anchor_charge"), 5, 800.f, 0.5f, 0.f },
			{ TEXT("beacon_power_select"), 5, 900.f, 0.6f, 0.f },
			{ TEXT("conduit_activate"), 3, 1.f, 0.f, 0.f },
			{ TEXT("enchantment_table_use"), 5, 500.f, 0.8f, 0.f },
			{ TEXT("anvil_use_fail"), 1, 300.f, 0.3f, 0.f },
			{ TEXT("enchant_ambient"), 5, 700.f, 0.6f, 0.f },
			{ TEXT("brewing_stand_brew"), 3, 0.8f, 0.f, 0.f },
			{ TEXT("cauldron_fill"), 3, 0.6f, 0.f, 0.f },
			{ TEXT("composter_fill"), 6, 400.f, 0.3f, 0.f },
			{ TEXT("composter_fill_success"), 6, 500.f, 0.35f, 0.f },
			{ TEXT("composter_ready"), 5, 700.f, 0.5f, 0.f },
			{ TEXT("composter_empty"), 6, 300.f, 0.3f, 0.f },
			{ TEXT("bone_meal_use"), 2, 1200.f, 0.25f, 0.f },
			{ TEXT("hoe_till"), 6, 700.f, 0.3f, 0.f },
			{ TEXT("shovel_flatten"), 2, 800.f, 0.3f, 0.f },
			{ TEXT("sheep_shear"), 7, 1100.f, 0.25f, 0.f },
			{ TEXT("lead_tied"), 7, 800.f, 0.3f, 0.f },
			{ TEXT("saddle_equip"), 7, 600.f, 0.35f, 0.f },
			{ TEXT("harness_equip"), 7, 620.f, 0.35f, 0.f },
			{ TEXT("donkey_chest"), 6, 300.f, 0.4f, 0.f },
			{ TEXT("mooshroom_milk"), 3, 0.5f, 0.f, 0.f },
			{ TEXT("chicken_egg"), 2, 700.f, 0.2f, 0.f },
			{ TEXT("fish_flop"), 3, 0.3f, 0.f, 0.f },
			{ TEXT("sweet_berry_pick"), 6, 900.f, 0.2f, 0.f },
			{ TEXT("cave_vines_pick"), 6, 850.f, 0.2f, 0.f },
			{ TEXT("amethyst_resonate"), 1, 1900.f, 0.8f, 5.f },
			{ TEXT("amethyst_chime"), 1, 2600.f, 0.6f, 6.f },
			{ TEXT("sculk_spread"), 5, 200.f, 0.7f, 0.f },
			{ TEXT("sculk_sensor_click"), 5, 1200.f, 0.2f, 0.f },
			{ TEXT("sculk_shrieker"), 4, 0.8f, 0.f, 0.f },
			{ TEXT("shelf_place"), 6, 400.f, 0.3f, 0.f },
			{ TEXT("book_put"), 6, 500.f, 0.3f, 0.f },
			{ TEXT("book_page_turn"), 7, 1400.f, 0.15f, 0.f },
			{ TEXT("map_create"), 7, 900.f, 0.3f, 0.f },
			{ TEXT("item_frame_place"), 6, 700.f, 0.25f, 0.f },
			{ TEXT("item_frame_break"), 6, 600.f, 0.25f, 0.f },
			{ TEXT("item_frame_add_item"), 6, 800.f, 0.2f, 0.f },
			{ TEXT("item_frame_remove_item"), 6, 700.f, 0.2f, 0.f },
			{ TEXT("item_frame_rotate_item"), 7, 1500.f, 0.15f, 0.f },
			{ TEXT("painting_place"), 7, 600.f, 0.3f, 0.f },
			{ TEXT("spyglass_use"), 1, 1400.f, 0.3f, 2.f },
			{ TEXT("totem_use"), 5, 400.f, 1.2f, 0.f },
			{ TEXT("goat_horn_sound"), 5, 150.f, 1.6f, 0.f },
			{ TEXT("goat_ram_impact"), 6, 200.f, 0.4f, 0.f },
			{ TEXT("wax_on"), 2, 1000.f, 0.25f, 0.f },
			{ TEXT("wax_off"), 2, 900.f, 0.25f, 0.f },
			{ TEXT("copper_wax_on"), 2, 1000.f, 0.25f, 0.f },
			{ TEXT("sponge_absorb"), 3, 0.6f, 0.f, 0.f },
			{ TEXT("concrete_harden"), 2, 600.f, 0.4f, 0.f },
			{ TEXT("crafter_craft"), 1, 700.f, 0.3f, 0.f },
			{ TEXT("crafter_fail"), 0, 300.f, 0.2f, 0.f },
			{ TEXT("vault_open"), 6, 300.f, 0.5f, 0.f },
			{ TEXT("vault_insert"), 1, 900.f, 0.3f, 0.f },
			{ TEXT("trial_spawner_spawn"), 5, 500.f, 0.6f, 0.f },
			{ TEXT("mob_spawner_spawn"), 2, 500.f, 0.5f, 0.f },
			{ TEXT("lightning_bolt_impact"), 4, 1.6f, 0.f, 0.f },
			{ TEXT("lightning_bolt_thunder"), 4, 2.f, 0.f, 0.f },
			{ TEXT("bell_resonate"), 1, 500.f, 1.4f, 5.f },
			{ TEXT("note_block_harp"), 6, 500.f, 0.6f, 0.f },
			{ TEXT("note_block_bass"), 6, 200.f, 0.8f, 0.f },
			{ TEXT("note_block_bell"), 1, 1000.f, 0.9f, 4.f },
			{ TEXT("note_block_chime"), 5, 1800.f, 0.6f, 0.f },
			{ TEXT("note_block_flute"), 2, 1600.f, 0.5f, 0.f },
			{ TEXT("note_block_guitar"), 6, 600.f, 0.6f, 0.f },
			{ TEXT("note_block_xylophone"), 5, 2200.f, 0.5f, 0.f },
			{ TEXT("note_block_iron_xylophone"), 1, 1200.f, 0.6f, 3.f },
			{ TEXT("note_block_cow_bell"), 1, 700.f, 0.7f, 4.f },
			{ TEXT("note_block_didgeridoo"), 2, 140.f, 1.0f, 0.f },
			{ TEXT("note_block_bit"), 5, 1000.f, 0.3f, 0.f },
			{ TEXT("note_block_banjo"), 6, 700.f, 0.5f, 0.f },
			{ TEXT("note_block_pling"), 5, 1500.f, 0.35f, 0.f },
			{ TEXT("note_block_snare"), 6, 1200.f, 0.2f, 0.f },
			{ TEXT("note_block_hat"), 7, 2400.f, 0.15f, 0.f },
			{ TEXT("note_block_basedrum"), 6, 200.f, 0.35f, 0.f },
			{ TEXT("music_stop"), 2, 300.f, 0.3f, 0.f },
			{ TEXT("player_attack_crit"), 1, 1200.f, 0.2f, 0.f },
			{ TEXT("player_attack_knockback"), 6, 300.f, 0.25f, 0.f },
			{ TEXT("player_attack_nodamage"), 6, 400.f, 0.2f, 0.f },
			{ TEXT("player_attack_sweep"), 7, 1000.f, 0.3f, 0.f },
			{ TEXT("player_attack_strong"), 6, 260.f, 0.3f, 0.f },
			{ TEXT("player_attack_weak"), 6, 500.f, 0.25f, 0.f },
			{ TEXT("player_hurt"), 2, 400.f, 0.3f, 0.f },
			{ TEXT("player_death"), 2, 300.f, 1.0f, 0.f },
			{ TEXT("player_eat"), 2, 500.f, 0.15f, 0.f },
			{ TEXT("player_drink"), 3, 0.3f, 0.f, 0.f },
			{ TEXT("raid_horn"), 5, 260.f, 1.2f, 0.f },
			{ TEXT("villager_yes"), 2, 700.f, 0.3f, 0.f },
			{ TEXT("villager_no"), 2, 500.f, 0.3f, 0.f },
			{ TEXT("villager_trade"), 5, 900.f, 0.4f, 0.f },
			{ TEXT("villager_work"), 6, 600.f, 0.3f, 0.f },
			{ TEXT("piglin_admiring_item"), 5, 700.f, 0.4f, 0.f },
			{ TEXT("zombie_attack_wooden_door"), 6, 300.f, 0.5f, 0.f },
			{ TEXT("zombie_infect"), 5, 400.f, 0.5f, 0.f },
			{ TEXT("zombie_break_door"), 6, 260.f, 0.6f, 0.f },
			{ TEXT("slime_attack"), 2, 300.f, 0.3f, 0.f },
			{ TEXT("iron_golem_attack"), 1, 260.f, 0.4f, 2.f },
			{ TEXT("iron_golem_repair"), 1, 700.f, 0.5f, 3.f },
			{ TEXT("snow_golem_shoot"), 2, 900.f, 0.25f, 0.f },
			{ TEXT("evoker_fangs_attack"), 5, 800.f, 0.25f, 0.f },
			{ TEXT("evoker_prepare_summon"), 5, 400.f, 0.8f, 0.f },
			{ TEXT("evoker_cast_spell"), 5, 600.f, 0.5f, 0.f },
			{ TEXT("illusioner_mirror_move"), 5, 1000.f, 0.4f, 0.f },
			{ TEXT("witch_laugh"), 5, 500.f, 0.8f, 0.f },
			{ TEXT("boat_paddle_water"), 3, 0.4f, 0.f, 0.f },
			{ TEXT("boat_hit"), 6, 400.f, 0.3f, 0.f },
			{ TEXT("boat_move"), 3, 0.2f, 0.f, 0.f },
			{ TEXT("minecart_rolling"), 1, 400.f, 0.3f, 0.f },
			{ TEXT("minecart_inside"), 1, 300.f, 0.4f, 0.f },
			{ TEXT("minecart_move"), 1, 500.f, 0.3f, 0.f },
			{ TEXT("fishing_bobber_throw"), 2, 700.f, 0.3f, 0.f },
			{ TEXT("fishing_bobber_splash"), 3, 1.f, 0.f, 0.f },
			{ TEXT("fishing_bobber_retrieve"), 2, 600.f, 0.3f, 0.f },
			{ TEXT("stonecutter_use"), 1, 1800.f, 0.4f, 1.f },
			{ TEXT("stonecutter_take_result"), 1, 900.f, 0.3f, 0.f },
			{ TEXT("grindstone_use"), 1, 1600.f, 0.6f, 1.f },
			{ TEXT("smithing_table_use"), 1, 900.f, 0.5f, 2.f },
			{ TEXT("cartography_table_take_result"), 7, 700.f, 0.25f, 0.f },
			{ TEXT("loom_take_result"), 7, 600.f, 0.25f, 0.f },
			{ TEXT("lectern_page_turn"), 7, 1300.f, 0.15f, 0.f },
			{ TEXT("jukebox_play"), 6, 300.f, 0.4f, 0.f },
			{ TEXT("jukebox_stop"), 6, 250.f, 0.3f, 0.f },
			{ TEXT("lodestone_compass_lock"), 1, 800.f, 0.6f, 4.f },
			{ TEXT("big_dripleaf_tilt_down"), 6, 400.f, 0.4f, 0.f },
			{ TEXT("big_dripleaf_tilt_up"), 6, 380.f, 0.4f, 0.f },
			{ TEXT("create_water"), 3, 0.7f, 0.f, 0.f },
			{ TEXT("create_fire"), 4, 0.5f, 0.f, 0.f },
			{ TEXT("creaking_sway"), 6, 200.f, 1.2f, 0.f },
			{ TEXT("creaking_heartbeat"), 4, 0.4f, 0.f, 0.f },
			{ TEXT("camel_dash"), 7, 320.f, 0.4f, 0.f },
			{ TEXT("horse_jump"), 7, 400.f, 0.35f, 0.f },
			{ TEXT("armadillo_scute"), 6, 700.f, 0.3f, 0.f },
			{ TEXT("allay_item_given"), 5, 1500.f, 0.4f, 0.f },
			{ TEXT("allay_item_taken"), 5, 1300.f, 0.4f, 0.f },
			{ TEXT("bottle_drink"), 3, 0.3f, 0.f, 0.f },
			{ TEXT("honey_block_slide"), 2, 200.f, 0.5f, 0.f },
			{ TEXT("slime_block_bounce"), 2, 250.f, 0.4f, 0.f },
			{ TEXT("candle_extinguish"), 4, 0.25f, 0.f, 0.f },
			{ TEXT("candle_ambient"), 4, 0.1f, 0.f, 0.f },
			{ TEXT("dripstone_drip"), 3, 0.15f, 0.f, 0.f },
			{ TEXT("pointed_dripstone_land"), 6, 700.f, 0.3f, 0.f },
			{ TEXT("spore_blossom_ambient"), 5, 300.f, 0.6f, 0.f },
			{ TEXT("amethyst_cluster_break"), 1, 2200.f, 0.5f, 5.f },
			{ TEXT("sculk_catalyst_bloom"), 5, 180.f, 1.2f, 0.f },
			{ TEXT("warden_emerge"), 4, 1.0f, 0.f, 0.f },
			{ TEXT("warden_roar"), 5, 90.f, 1.8f, 0.f },
			{ TEXT("wither_ambient"), 5, 110.f, 1.4f, 0.f },
			{ TEXT("wither_death"), 5, 140.f, 1.6f, 0.f },
			{ TEXT("ender_dragon_ambient"), 5, 130.f, 1.6f, 0.f },
			{ TEXT("ender_dragon_hurt"), 5, 160.f, 1.0f, 0.f },
			{ TEXT("ender_dragon_death"), 5, 90.f, 2.0f, 0.f },
			{ TEXT("ender_dragon_fireball"), 5, 200.f, 0.9f, 0.f },
			{ TEXT("end_portal_ambient"), 5, 150.f, 1.6f, 0.f },
			{ TEXT("respawn_anchor_ambient"), 5, 220.f, 1.4f, 0.f },
			{ TEXT("conduit_ambient"), 3, 0.8f, 0.f, 0.f },
			{ TEXT("beacon_ambient"), 5, 700.f, 0.8f, 0.f },
			{ TEXT("beacon_deactivate"), 5, 400.f, 0.5f, 0.f },
			{ TEXT("beacon_activate"), 5, 900.f, 0.7f, 0.f },
			{ TEXT("beacon_power"), 5, 1000.f, 0.5f, 0.f },
			{ TEXT("xporb_bottle_break"), 5, 1400.f, 0.3f, 0.f },
			{ TEXT("night_vision_on"), 5, 700.f, 0.5f, 0.f }
		};
		for (const FOnce& O : Table)
		{
			if (N != O.Id) continue;
			Ensure(O.P1 + 1.4f);
			Out.Samples.SetNumZeroed(Len);
			const float V = S.R.FRange(0.9f, 1.1f);
			switch (O.Kind)
			{
			case 0: // UI click
				Noise(S, 0.f, O.P1, 0.6f * V, O.P0, O.P0 * 0.4f, 40.f, true);
				Tone(S, 0.f, O.P1, O.P0 * 0.5f, O.P0 * 0.4f, FWave::Square, 0.25f * V, 45.f);
				break;
			case 1: // metallic
				for (int32 k = 0; k < 4; ++k) FM(S, 0.f, O.P1, O.P0 * (1.f + k * 0.53f) * V, 1.41f + k * 0.2f, 3.f, 0.35f * V / (1.f + k * 0.6f), 12.f + k * 6.f);
				Noise(S, 0.f, O.P1 * 0.4f, 0.3f * V, O.P0 * 3.f, O.P0 * 1.2f, 30.f, true);
				break;
			case 2: // soft / cloth
				Noise(S, 0.f, O.P1, 0.5f * V, O.P0, O.P0 * 0.45f, 22.f);
				break;
			case 3: // water
				Noise(S, 0.f, O.P1, 0.6f * V, 2400.f, 400.f, 10.f);
				Noise(S, 0.02f, O.P1 * 1.4f, 0.3f * V, 900.f, 250.f, 6.f);
				Tone(S, 0.f, O.P1 * 0.6f, 220.f, 90.f, FWave::Sine, 0.2f * V, 18.f);
				break;
			case 4: // explosive / rumble
				Explode(S, 0.75f * V * FMath::Max(0.4f, O.P0), FMath::Max(0.4f, O.P0));
				break;
			case 5: // magic
			{
				const float F = O.P0 * V;
				Tone(S, 0.f, O.P1, F, F * 1.6f, FWave::Sine, 0.35f * V, 8.f, 0.01f, 0.03f);
				Tone(S, 0.02f, O.P1, F * 1.5f, F * 0.8f, FWave::Sine, 0.22f * V, 10.f);
				Noise(S, 0.f, O.P1, 0.18f * V, F * 3.f, F, 12.f);
				break;
			}
			case 6: // wood / percussive
				Noise(S, 0.f, O.P1, 0.55f * V, O.P0 * 1.8f, O.P0 * 0.5f, 26.f);
				Tone(S, 0.f, O.P1 * 0.7f, O.P0 * 0.6f * V, O.P0 * 0.35f, FWave::Triangle, 0.35f * V, 30.f);
				break;
			default: // cloth / armour
				Noise(S, 0.f, O.P1, 0.45f * V, O.P0 * 1.4f, O.P0 * 0.6f, 18.f);
				Tone(S, 0.f, O.P1 * 0.5f, O.P0 * 0.4f, O.P0 * 0.3f, FWave::Sine, 0.2f * V, 25.f);
				break;
			}
			return;
		}

		// ---------------- fallback: a short generic blip so nothing is silent
		Ensure(0.3f);
		Out.Samples.SetNumZeroed(Len);
		Tone(S, 0.f, 0.18f, 320.f * S.R.FRange(0.8f, 1.3f), 180.f, FWave::Triangle, 0.45f, 18.f);
		Noise(S, 0.f, 0.12f, 0.2f, 1400.f, 500.f, 30.f);
	}

	void GenerateLoop(FName Loop, FMCSoundClip& Out)
	{
		Out.SampleRate = 32000;
		const FString N = Loop.ToString();
		const float Len = N == TEXT("music") ? 4.f : 3.f;
		const int32 SR = Out.SampleRate;
		Out.Samples.SetNumZeroed((int32)(Len * SR));
		FSyn S(Out);
		S.R.SetSeed((uint32)HashName(N) * 13 + 5);
		const int32 N0 = Out.Samples.Num();
		if (N.Contains(TEXT("rain")))
		{
			Noise(S, 0.f, Len, 0.32f, 3200.f, 2600.f, 0.2f);
			Noise(S, 0.f, Len, 0.2f, 900.f, 700.f, 0.1f);
		}
		else if (N.Contains(TEXT("thunder")))
		{
			Noise(S, 0.f, Len, 0.35f, 1800.f, 900.f, 0.2f);
			Noise(S, 0.f, Len, 0.25f, 140.f, 90.f, 0.1f);
		}
		else if (N.Contains(TEXT("cave")))
		{
			Noise(S, 0.f, Len, 0.28f, 320.f, 200.f, 0.05f);
			Tone(S, 0.f, Len, 62.f, 58.f, FWave::Sine, 0.18f, 0.f, 0.4f, 0.01f, true);
			Tone(S, 0.f, Len, 93.f, 96.f, FWave::Sine, 0.09f, 0.f, 0.6f, 0.008f, true);
		}
		else if (N.Contains(TEXT("nether")))
		{
			Noise(S, 0.f, Len, 0.16f, 220.f, 160.f, 0.05f);
			Tone(S, 0.f, Len, 44.f, 42.f, FWave::Saw, 0.14f, 0.f, 0.5f, 0.01f, true);
			Tone(S, 0.f, Len, 66.f, 69.f, FWave::Triangle, 0.08f, 0.f, 0.7f, 0.006f, true);
		}
		else if (N.Contains(TEXT("end")))
		{
			Tone(S, 0.f, Len, 110.f, 108.f, FWave::Sine, 0.2f, 0.f, 0.8f, 0.004f, true);
			Tone(S, 0.f, Len, 165.f, 168.f, FWave::Sine, 0.1f, 0.f, 0.9f, 0.003f, true);
			Noise(S, 0.f, Len, 0.08f, 400.f, 300.f, 0.05f);
		}
		else if (N.Contains(TEXT("underwater")))
		{
			Noise(S, 0.f, Len, 0.3f, 500.f, 260.f, 0.2f);
			Tone(S, 0.f, Len, 120.f, 118.f, FWave::Sine, 0.12f, 0.f, 0.5f, 0.006f, true);
		}
		else if (N.Contains(TEXT("portal")))
		{
			Noise(S, 0.f, Len, 0.18f, 1200.f, 900.f, 0.1f);
			Tone(S, 0.f, Len, 180.f, 182.f, FWave::Sine, 0.14f, 0.f, 0.5f, 0.02f, true);
			Tone(S, 0.f, Len, 271.f, 268.f, FWave::Sine, 0.08f, 0.f, 0.5f, 0.015f, true);
		}
		else if (N.Contains(TEXT("water")))
		{
			Noise(S, 0.f, Len, 0.28f, 1600.f, 1200.f, 0.2f);
		}
		else if (N.Contains(TEXT("wind")) || N.Contains(TEXT("high")))
		{
			Noise(S, 0.f, Len, 0.22f, 700.f, 500.f, 0.1f);
		}
		else
		{
			Noise(S, 0.f, Len, 0.2f, 800.f, 600.f, 0.1f);
		}
		// crossfade the ends for a seamless loop
		const int32 Fade = FMath::Min(N0 / 4, (int32)(0.25f * SR));
		for (int32 i = 0; i < Fade; ++i)
		{
			const float T = i / (float)Fade;
			const int32 A = i, BIdx = N0 - Fade + i;
			const int32 Mixed = (int32)(Out.Samples[A] * T + Out.Samples[BIdx] * (1.f - T));
			Out.Samples[A] = (int16)Mixed;
			Out.Samples[BIdx] = (int16)Mixed;
		}
	}

	void GenerateMusic(int32 Track, EMCDimension Dim, FMCSoundClip& Out)
	{
		Out.SampleRate = 32000;
		const int32 SR = Out.SampleRate;
		const float Len = 48.f;
		Out.Samples.SetNumZeroed((int32)(Len * SR));
		FSyn S(Out);
		S.R.SetSeed((uint32)(Track * 977 + (int32)Dim * 31 + 7));

		// scale selection per dimension (original themes, no borrowed melodies)
		static const int32 MinorS[7] = { 0, 2, 3, 5, 7, 8, 10 };
		static const int32 DorianS[7] = { 0, 2, 3, 5, 7, 9, 10 };
		static const int32 PentaS[5] = { 0, 3, 5, 7, 10 };
		const int32* Scale = Dim == EMCDimension::Nether ? MinorS : (Dim == EMCDimension::End ? PentaS : DorianS);
		const int32 ScaleLen = Dim == EMCDimension::End ? 5 : 7;
		const float RootHz = Dim == EMCDimension::Overworld ? 220.f : (Dim == EMCDimension::Nether ? 138.6f : 164.8f);
		const float BarLen = 4.f;
		const int32 Bars = (int32)(Len / BarLen);

		// bass + chord bed
		for (int32 b = 0; b < Bars; ++b)
		{
			const int32 Deg = Scale[(S.R.NextInt(4) + b) % ScaleLen];
			const float F = RootHz * FMath::Pow(2.f, Deg / 12.f) * (b % 4 == 3 ? 0.5f : 0.25f);
			const float Freqs[3] = { F, F * 1.5f, F * 2.f };
			Pad(S, b * BarLen, BarLen * 1.02f, Freqs, 3, b % 2 ? 0.22f : 0.3f, 0.35f, 0.6f, FWave::Triangle, 0.004f);
		}
		// melody
		float T = 0.f;
		while (T < Len - 1.f)
		{
			const float NoteLen = FMath::Lerp(0.5f, 2.0f, S.R.NextFloat());
			const int32 Deg = Scale[S.R.NextInt(ScaleLen)];
			const int32 Oct = S.R.NextInt(2);
			const float F = RootHz * 2.f * FMath::Pow(2.f, Deg / 12.f) * (Oct ? 2.f : 1.f);
			Tone(S, T, NoteLen * 0.95f, F, F, S.R.NextInt(2) ? FWave::Sine : FWave::Triangle, 0.16f, S.R.FRange(0.6f, 1.6f), 0.03f, 0.004f);
			if (S.R.NextInt(4) == 0) Tone(S, T + 0.05f, NoteLen * 0.5f, F * 1.5f, F * 1.5f, FWave::Sine, 0.08f, 1.4f, 0.02f);
			T += NoteLen * S.R.FRange(0.6f, 1.2f);
		}
		// percussion for the nether theme
		if (Dim == EMCDimension::Nether)
			for (int32 b = 0; b < Bars * 4; ++b)
			{
				const float P = b * (BarLen / 4.f);
				if (b % 4 == 0) { Noise(S, P, 0.3f, 0.35f, 300.f, 90.f, 12.f); Tone(S, P, 0.35f, 60.f, 40.f, FWave::Sine, 0.4f, 8.f, 0.002f); }
				else if (b % 4 == 2) Noise(S, P, 0.2f, 0.18f, 2400.f, 1400.f, 26.f, true);
			}
		if (Dim == EMCDimension::End)
			for (int32 b = 0; b < Bars; ++b) Bell(S, b * BarLen + 1.5f, RootHz * 4.f * FMath::Pow(2.f, Scale[S.R.NextInt(ScaleLen)] / 12.f), 0.09f, 0.7f);
		// gentle fade in / out so the track can loop or stop cleanly
		for (int32 i = 0; i < SR * 3; ++i)
		{
			const float F = i / (float)(SR * 3);
			Out.Samples[i] = (int16)(Out.Samples[i] * F);
			Out.Samples[Out.Samples.Num() - 1 - i] = (int16)(Out.Samples[Out.Samples.Num() - 1 - i] * F);
		}
	}

	void EncodeWav(const FMCSoundClip& Clip, TArray<uint8>& OutBytes)
	{
		const int32 DataSize = Clip.Samples.Num() * 2;
		OutBytes.SetNumUninitialized(44 + DataSize);
		uint8* P = OutBytes.GetData();
		auto W32 = [&](int32 V) { FMemory::Memcpy(P, &V, 4); P += 4; };
		auto W16 = [&](int16 V) { FMemory::Memcpy(P, &V, 2); P += 2; };
		FMemory::Memcpy(P, "RIFF", 4); P += 4;
		W32(36 + DataSize);
		FMemory::Memcpy(P, "WAVEfmt ", 8); P += 8;
		W32(16);
		W16(1);
		W16(1);
		W32(Clip.SampleRate);
		W32(Clip.SampleRate * 2);
		W16(2);
		W16(16);
		FMemory::Memcpy(P, "data", 4); P += 4;
		W32(DataSize);
		FMemory::Memcpy(P, Clip.Samples.GetData(), DataSize);
	}
}
