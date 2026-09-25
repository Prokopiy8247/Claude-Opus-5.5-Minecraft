// Audio system: procedural clip cache, playback with distance attenuation, music / ambience / weather loops.
#include "Audio/MCAudio.h"
#include "Game/MCGame.h"
#include "Game/MCPlayer.h"
#include "World/MCWorld.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MCAudio)

UMCGeneratedWave::UMCGeneratedWave(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NumChannels = 1;
	Duration = INDEFINITELY_LOOPING_DURATION;
	bLooping = false;
	bProcedural = true;
	SoundGroup = SOUNDGROUP_Default;
}

void UMCGeneratedWave::SetClip(const FMCSoundClip& InClip, bool bLoop, int32 InSampleRate)
{
	Clip = InClip;
	bLooping = bLoop;
	SampleRate = (InSampleRate > 0) ? InSampleRate : InClip.SampleRate;
	NumChannels = 1;
	Duration = bLoop ? INDEFINITELY_LOOPING_DURATION : Clip.Duration();
	ReadPos = 0;
	bReady = Clip.Samples.Num() > 0;
	NumSamplesToGeneratePerCallback = 1024;
	ResetAudio();
	if (bReady)
	{
		// pre-queue a few hundred milliseconds so one shots never start with a gap
		TArray<uint8> Bytes;
		const int32 Pre = FMath::Min(Clip.Samples.Num(), (int32)(0.3f * SampleRate));
		Bytes.SetNumUninitialized(Pre * 2);
		FMemory::Memcpy(Bytes.GetData(), Clip.Samples.GetData(), Pre * 2);
		QueueAudio(Bytes.GetData(), Bytes.Num());
		ReadPos = Pre;
	}
}

int32 UMCGeneratedWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	if (!bReady) return 0;
	const int32 Remaining = Clip.Samples.Num() - ReadPos;
	if (Remaining <= 0)
	{
		if (!bLooping) return 0;
		ReadPos = 0;
		const int32 Wrap = FMath::Min(Clip.Samples.Num(), NumSamples);
		OutAudio.SetNumUninitialized(Wrap * 2);
		FMemory::Memcpy(OutAudio.GetData(), Clip.Samples.GetData(), Wrap * 2);
		ReadPos = Wrap;
		return Wrap;
	}
	const int32 Take = FMath::Min(Remaining, NumSamples);
	OutAudio.SetNumUninitialized(Take * 2);
	FMemory::Memcpy(OutAudio.GetData(), Clip.Samples.GetData() + ReadPos, Take * 2);
	ReadPos += Take;
	return Take;
}

// ---------------------------------------------------------------------------------------------------------------------

void UMCAudio::Init(AMCGame* InGame)
{
	Game = InGame;
	Rand.SetSeed(0x51A0D1u);
	Attenuation = NewObject<USoundAttenuation>(this, TEXT("MCAtt"));
	Attenuation->Attenuation.bAttenuate = true;
	Attenuation->Attenuation.bSpatialize = true;
	Attenuation->Attenuation.AttenuationShapeExtents = FVector(1800.f, 0.f, 0.f); // 18 blocks to full volume
	Attenuation->Attenuation.FalloffDistance = 9000.f;
	Attenuation->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
	Attenuation->Attenuation.bAttenuateWithLPF = true;
	Attenuation->Attenuation.LPFRadiusMin = 4000.f;
	Attenuation->Attenuation.LPFRadiusMax = 12000.f;
	NextMusicTime = FPlatformTime::Seconds() + Rand.FRange(20.0, 60.0);
	if (Game && Game->Audio) Game->Audio = this;
	UE_LOG(LogOpus55, Log, TEXT("Audio initialised (procedural synthesis)"));
}

UMCGeneratedWave* UMCAudio::GetWave(FName Sound, int32 Variant)
{
	const FName Key = FName(*FString::Printf(TEXT("%s#%d"), *Sound.ToString(), Variant));
	FMCSoundClip* Clip = ClipCache.Find(Key);
	if (!Clip)
	{
		const double T0 = FPlatformTime::Seconds();
		FMCSoundClip New;
		MCSoundSynth::Generate(Sound, Variant, New);
		Clip = &ClipCache.Add(Key, MoveTemp(New));
		if (ClipCache.Num() <= 8 || ClipCache.Num() % 64 == 0)
			UE_LOG(LogOpus55, Log, TEXT("Synthesised '%s' variant %d (%.2fs, %.1fms, %d clips cached)"), *Sound.ToString(), Variant, Clip->Duration(), (FPlatformTime::Seconds() - T0) * 1000.0, ClipCache.Num());
	}
	UMCGeneratedWave* W = NewObject<UMCGeneratedWave>(this);
	W->SetClip(*Clip, false, Clip->SampleRate);
	return W;
}

void UMCAudio::OnMusicFinished(UAudioComponent* Comp)
{
	if (Comp && Comp == MusicComp) { MusicComp = nullptr; }
	if (Comp && Comp == DiscComp) { DiscComp = nullptr; }
}

UMCGeneratedWave* UMCAudio::GetLoopWave(FName Loop)
{
	if (TObjectPtr<UMCGeneratedWave>* Found = LoopWaves.Find(Loop)) return Found->Get();
	UMCGeneratedWave* W = NewObject<UMCGeneratedWave>(this);
	FMCSoundClip Clip;
	MCSoundSynth::GenerateLoop(Loop, Clip);
	W->SetClip(Clip, true, Clip.SampleRate);
	W->bLooping = true;
	LoopWaves.Add(Loop, W);
	return W;
}

void UMCAudio::Play(FName Sound, const FVector& PosBlocks, float Volume, float Pitch)
{
	if (!Game || !GetOuter() || Volume <= 0.f) return;
	UWorld* W = Game->GetWorld();
	if (!W) return;
	// rate limiting: identical sounds close together are collapsed, plus a hard cap per tick
	const double Now = FPlatformTime::Seconds();
	if (double* Last = LastPlayed.Find(Sound))
	{
		if (Now - *Last < 0.02) return;
		*Last = Now;
	}
	else LastPlayed.Add(Sound, Now);
	if (PlayedThisTick > 24) return;

	const int32 Variants = MCSoundSynth::NumVariants(Sound);
	UMCGeneratedWave* Wave = GetWave(Sound, Rand.NextInt(FMath::Max(1, Variants)));
	if (!Wave) return;
	const FVector UU = PosBlocks * MC::BlockSize;
	UGameplayStatics::PlaySoundAtLocation(W, Wave, UU, Volume * MasterVolume * BlockVolume, FMath::Clamp(Pitch, 0.3f, 2.5f), 0.f, Attenuation);
	++PlayedThisTick;
}

void UMCAudio::Play2D(FName Sound, float Volume, float Pitch)
{
	if (!Game) return;
	UWorld* W = Game->GetWorld();
	if (!W) return;
	const int32 Variants = MCSoundSynth::NumVariants(Sound);
	UMCGeneratedWave* Wave = GetWave(Sound, Rand.NextInt(FMath::Max(1, Variants)));
	if (!Wave) return;
	UGameplayStatics::PlaySound2D(W, Wave, Volume * MasterVolume, FMath::Clamp(Pitch, 0.3f, 2.5f));
}

UAudioComponent* UMCAudio::MakeLoopComponent(UMCGeneratedWave* Wave, float Volume)
{
	if (!Game || !Wave) return nullptr;
	UWorld* W = Game->GetWorld();
	if (!W) return nullptr;
	UAudioComponent* C = UGameplayStatics::SpawnSound2D(W, Wave, Volume, 1.f, 0.f, nullptr, true, false);
	if (C)
	{
		C->bAutoDestroy = false;
		C->bIsUISound = true;
		C->Play();
	}
	return C;
}

void UMCAudio::Tick(float DeltaSeconds)
{
	SoundsThisSecond = PlayedThisTick;
	PlayedThisTick = 0;
	Clock += DeltaSeconds;
	if (!Game) return;

	// listener follows the player camera
	if (Game->Player)
	{
		const FVector UU = Game->Player->Pos * MC::BlockSize;
		// attenuation objects are evaluated per sound; nothing else needed here
		(void)UU;
	}

	UpdateAmbience();

	// music scheduling (original generated tracks, one per dimension, drifting in every few minutes)
	const double Now = FPlatformTime::Seconds();
	if (Now >= NextMusicTime && MusicVolume > 0.001f && !MusicComp)
	{
		UMCGeneratedWave* Track = NewObject<UMCGeneratedWave>(this);
		FMCSoundClip Clip;
		MCSoundSynth::GenerateMusic(MusicTrack, Game->ActiveDim, Clip);
		Track->SetClip(Clip, false, Clip.SampleRate);
		UWorld* W = Game->GetWorld();
		if (W)
		{
			if (MusicComp) { MusicComp->Stop(); MusicComp->DestroyComponent(); }
			MusicComp = UGameplayStatics::SpawnSound2D(W, Track, MusicVolume * MasterVolume, 1.f, 0.f, nullptr, false, false);
			if (MusicComp)
			{
				MusicComp->bAutoDestroy = false;
				MusicComp->bIsUISound = true;
				MusicComp->OnAudioFinishedNative.AddUObject(this, &UMCAudio::OnMusicFinished);
				MusicComp->Play();
			}
		}
		++MusicTrack;
		NextMusicTime = Now + Clip.Duration() + Rand.FRange(90.0, 300.0);
	}
}

void UMCAudio::UpdateAmbience()
{
	if (!Game) return;
	FMCWorld* W = Game->ActiveWorld();
	if (!W) return;
	FName Want = NAME_None;
	if (W->Dim == EMCDimension::Nether) Want = TEXT("loop_nether");
	else if (W->Dim == EMCDimension::End) Want = TEXT("loop_end");
	else Want = TEXT("loop_cave");

	if (AmbientVolume <= 0.001f) { if (AmbientComp) AmbientComp->SetVolumeMultiplier(0.f); }
	else
	{
		if (CurrentAmbient != Want)
		{
			if (AmbientComp) { AmbientComp->Stop(); AmbientComp->DestroyComponent(); AmbientComp = nullptr; }
			AmbientComp = MakeLoopComponent(GetLoopWave(Want), AmbientVolume * MasterVolume * 0.35f);
			CurrentAmbient = Want;
		}
		else if (AmbientComp)
		{
			AmbientComp->SetVolumeMultiplier(AmbientVolume * MasterVolume * 0.35f);
			if (!AmbientComp->IsPlaying()) AmbientComp->Play();
		}
	}

	// weather loop
	const bool bWet = Game->RainLevel > 0.25f && W->Dim == EMCDimension::Overworld;
	if (bWet && !RainComp) RainComp = MakeLoopComponent(GetLoopWave(TEXT("loop_rain")), 0.3f * MasterVolume);
	else if (!bWet && RainComp) { RainComp->Stop(); RainComp->DestroyComponent(); RainComp = nullptr; }
	else if (RainComp) RainComp->SetVolumeMultiplier(0.3f * MasterVolume * FMath::Clamp(Game->RainLevel, 0.f, 1.f));
}

void UMCAudio::StopAll()
{
	if (MusicComp) { MusicComp->Stop(); MusicComp->DestroyComponent(); MusicComp = nullptr; }
	if (AmbientComp) { AmbientComp->Stop(); AmbientComp->DestroyComponent(); AmbientComp = nullptr; }
	if (RainComp) { RainComp->Stop(); RainComp->DestroyComponent(); RainComp = nullptr; }
	if (DiscComp) { DiscComp->Stop(); DiscComp->DestroyComponent(); DiscComp = nullptr; }
	CurrentAmbient = NAME_None;
}

void UMCAudio::PlayMusicDisc(FName Disc, const FVector& PosBlocks)
{
	if (!Game) return;
	UWorld* W = Game->GetWorld();
	if (!W) return;
	StopMusicDisc();
	UMCGeneratedWave* Track = NewObject<UMCGeneratedWave>(this);
	FMCSoundClip Clip;
	MCSoundSynth::GenerateMusic(100 + (int32)(MCHash::StringHash(*Disc.ToString()) % 4), Game->ActiveDim, Clip);
	Track->SetClip(Clip, false, Clip.SampleRate);
	DiscComp = UGameplayStatics::SpawnSoundAtLocation(W, Track, PosBlocks * MC::BlockSize, FRotator::ZeroRotator, MusicVolume * MasterVolume, 1.f, 0.f, Attenuation, nullptr, false);
	if (DiscComp)
	{
		DiscComp->bAutoDestroy = false;
		DiscComp->OnAudioFinishedNative.AddUObject(this, &UMCAudio::OnMusicFinished);
	}
}

void UMCAudio::StopMusicDisc()
{
	if (DiscComp) { DiscComp->Stop(); DiscComp->DestroyComponent(); DiscComp = nullptr; }
	if (MusicComp && !MusicComp->IsPlaying()) { MusicComp->DestroyComponent(); MusicComp = nullptr; }
}
