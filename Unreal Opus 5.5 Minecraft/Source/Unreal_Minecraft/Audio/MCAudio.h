// Audio: every sound is synthesised procedurally (original, deterministic) into PCM once and played through
// transient sound waves; baked USoundWave assets under /Game/Opus55Minecraft/Audio are preferred when present.
// Also runs music, cave/nether/end ambience and weather loops.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/MCCore.h"
#include "Sound/SoundWaveProcedural.h"
#include "MCAudio.generated.h"

class AMCGame;
class USoundWave;
class USoundBase;
class UAudioComponent;
class USoundAttenuation;
class USoundWaveProcedural;

/** Synthesised PCM (mono 16 bit). */
struct FMCSoundClip
{
	TArray<int16> Samples;
	int32 SampleRate = 32000;
	float Duration() const { return Samples.Num() / (float)SampleRate; }
};

namespace MCSoundSynth
{
	/** Build the clip for a sound id (variant selects a random alternative). Always returns something audible. */
	UNREAL_MINECRAFT_API void Generate(FName Sound, int32 Variant, FMCSoundClip& Out);
	/** Number of variants for a sound id (random pick when playing). */
	UNREAL_MINECRAFT_API int32 NumVariants(FName Sound);
	/** Background music track (long, generated in chunks). */
	UNREAL_MINECRAFT_API void GenerateMusic(int32 Track, EMCDimension Dim, FMCSoundClip& Out);
	/** Loops (rain, cave wind, nether drone, end hum, portal). */
	UNREAL_MINECRAFT_API void GenerateLoop(FName Loop, FMCSoundClip& Out);
	/** WAV (RIFF) encoding for asset baking. */
	UNREAL_MINECRAFT_API void EncodeWav(const FMCSoundClip& Clip, TArray<uint8>& OutBytes);
}

/** Procedurally generated PCM stream: either one shot (fully queued) or looping (refilled on demand). */
UCLASS()
class UNREAL_MINECRAFT_API UMCGeneratedWave : public USoundWaveProcedural
{
	GENERATED_BODY()
public:
	UMCGeneratedWave(const FObjectInitializer& ObjectInitializer);

	/** Fills the wave with a synthesised clip (call before playing). */
	void SetClip(const FMCSoundClip& Clip, bool bLoop, int32 InSampleRate);
	/** Refills the queue after playback started (looping). */
	void Refill();

	int32 ReadPos = 0;
	bool bLooping = false;

protected:
	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

private:
	FMCSoundClip Clip;
	bool bReady = false;
};

UCLASS()
class UNREAL_MINECRAFT_API UMCAudio : public UObject
{
	GENERATED_BODY()
public:
	void Init(AMCGame* InGame);
	/** 3D sound at a world position (block units). */
	void Play(FName Sound, const FVector& PosBlocks, float Volume = 1.f, float Pitch = 1.f);
	/** Non-spatialised UI / player sound. */
	void Play2D(FName Sound, float Volume = 1.f, float Pitch = 1.f);
	/** Per-frame: listener, music scheduling, ambient loops. */
	void Tick(float DeltaSeconds);
	void StopAll();
	void PlayMusicDisc(FName Disc, const FVector& PosBlocks);
	void StopMusicDisc();

	float MasterVolume = 1.f;
	float MusicVolume = 0.5f;
	float BlockVolume = 1.f;
	float MobVolume = 1.f;
	float AmbientVolume = 0.8f;
	int32 SoundsThisSecond = 0;
	/** Generated PCM clips, keyed by id#variant (read-only; exposed for the F3 debug overlay). */
	const TMap<FName, FMCSoundClip>& GetClipCache() const { return ClipCache; }

private:
	UPROPERTY(Transient) TObjectPtr<AMCGame> Game = nullptr;
	TMap<FName, FMCSoundClip> ClipCache;                                     // key = id#variant (PCM shared by all plays)
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UMCGeneratedWave>> LoopWaves;  // key = loop name
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> Attenuation;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> MusicComp;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> AmbientComp;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> RainComp;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> DiscComp;
	FName CurrentAmbient;
	FMCRandom Rand;
	double NextMusicTime = 60.0;
	double Clock = 0.0;
	int32 MusicTrack = 0;
	int32 PlayedThisTick = 0;
	TMap<FName, double> LastPlayed;

	/** A fresh wave instance for one playback (procedural waves cannot be shared between voices). */
	UMCGeneratedWave* GetWave(FName Sound, int32 Variant);
	void OnMusicFinished(UAudioComponent* Comp);
	UMCGeneratedWave* GetLoopWave(FName Loop);
	UAudioComponent* MakeLoopComponent(UMCGeneratedWave* Wave, float Volume);
	void UpdateAmbience();
};
