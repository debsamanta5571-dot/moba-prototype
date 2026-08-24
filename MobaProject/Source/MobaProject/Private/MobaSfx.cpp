#include "MobaSfx.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MobaGameInstance.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundGroups.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	struct FSynth
	{
		uint32 S = 0xA341316Cu;
		float Lp = 0.f;

		float Noise()
		{
			S = S * 1664525u + 1013904223u;
			return (static_cast<int32>(S >> 8) / 8388608.f);
		}

		float Lowpass(float In, float Cut)
		{
			Lp += Cut * (In - Lp);
			return Lp;
		}
	};

	float EnvAR(float T, float Attack, float Release, float Dur)
	{
		if (T < Attack)
		{
			return Attack > 0.f ? T / Attack : 1.f;
		}
		const float Tail = Dur - T;
		if (Tail < Release)
		{
			return Release > 0.f ? FMath::Max(0.f, Tail / Release) : 0.f;
		}
		return 1.f;
	}

	float Punch(float T, FSynth& Synth, float Pitch)
	{
		const float Thump = FMath::Sin(2.f * PI * (58.f * Pitch) * T) * FMath::Exp(-T * 18.f);
		const float Body = FMath::Sin(2.f * PI * (112.f * Pitch) * T) * FMath::Exp(-T * 22.f);
		const float Slap = FMath::Sin(2.f * PI * (310.f * Pitch) * T) * FMath::Exp(-T * 34.f);
		const float Flesh = Synth.Noise() * FMath::Exp(-T * 38.f);
		float Sample = Thump * 0.9f + Body * 0.55f + Slap * 0.32f + Flesh * 0.42f;
		if (T < 0.012f)
		{
			Sample *= 1.8f;
		}
		return FMath::Clamp(Sample * 1.35f, -1.f, 1.f);
	}

	float SampleSfx(EMobaSfx Kind, float T, FSynth& Synth)
	{
		switch (Kind)
		{
		case EMobaSfx::MeleeCast:
			return Punch(T, Synth, 1.08f) * 0.9f;
		case EMobaSfx::MeleeHit:
			return Punch(T, Synth, 0.92f);
		case EMobaSfx::Dash:
		{
			const float Dur = 0.28f;
			const float Rush = Synth.Noise() * EnvAR(T, 0.02f, 0.2f, Dur);
			const float Lift = FMath::Sin(2.f * PI * (220.f + 900.f * T) * T) * EnvAR(T, 0.01f, 0.18f, Dur) * 0.18f;
			return (Rush * 0.62f + Lift) * 0.75f;
		}
		case EMobaSfx::SkillshotFire:
		{
			const float Dur = 0.15f;
			const float Freq = 520.f + 1600.f * (T / Dur);
			const float Zap = FMath::Sin(2.f * PI * Freq * T) * FMath::Exp(-T * 11.f);
			const float Click = Synth.Noise() * FMath::Exp(-T * 40.f) * 0.35f;
			return (Zap * 0.7f + Click) * 0.8f;
		}
		case EMobaSfx::GroundBlast:
		{
			const float Crack = Synth.Noise() * FMath::Exp(-T * 48.f);
			const float Debris = Synth.Lowpass(Synth.Noise(), 0.18f) * FMath::Exp(-T * 5.5f);
			const float Sub = FMath::Sin(2.f * PI * 36.f * T) * FMath::Exp(-T * 3.6f);
			const float Boom = FMath::Sin(2.f * PI * 62.f * T) * FMath::Exp(-T * 4.8f);
			const float Mid = FMath::Sin(2.f * PI * 128.f * T) * FMath::Exp(-T * 10.f);
			float Sample = Sub * 0.95f + Boom * 0.8f + Mid * 0.28f + Crack * 0.75f + Debris * 0.55f;
			if (T < 0.05f)
			{
				Sample *= 1.7f;
			}
			return FMath::Clamp(Sample, -1.f, 1.f);
		}
		case EMobaSfx::ProjectileDestroy:
		{
			const float Pop = FMath::Sin(2.f * PI * 1700.f * T) * FMath::Exp(-T * 42.f);
			const float Spark = Synth.Noise() * FMath::Exp(-T * 36.f);
			return (Pop * 0.45f + Spark * 0.55f) * 0.75f;
		}
		case EMobaSfx::TowerFire:
		{
			const float Thump = FMath::Sin(2.f * PI * 46.f * T) * FMath::Exp(-T * 9.f);
			const float Zap = FMath::Sin(2.f * PI * (380.f + 900.f * T) * T) * FMath::Exp(-T * 13.f);
			const float Air = Synth.Noise() * FMath::Exp(-T * 16.f);
			return FMath::Clamp(Thump * 0.9f + Zap * 0.38f + Air * 0.32f, -1.f, 1.f);
		}
		default:
			return 0.f;
		}
	}

	float DurationFor(EMobaSfx Kind)
	{
		switch (Kind)
		{
		case EMobaSfx::MeleeCast: return 0.14f;
		case EMobaSfx::MeleeHit: return 0.18f;
		case EMobaSfx::Dash: return 0.28f;
		case EMobaSfx::SkillshotFire: return 0.15f;
		case EMobaSfx::GroundBlast: return 0.78f;
		case EMobaSfx::ProjectileDestroy: return 0.09f;
		case EMobaSfx::TowerFire: return 0.28f;
		default: return 0.f;
		}
	}

	USoundWaveProcedural* MakeWave(EMobaSfx Kind)
	{
		const float Duration = DurationFor(Kind);
		if (Duration <= 0.f)
		{
			return nullptr;
		}

		const int32 SampleRate = 44100;
		const int32 NumSamples = FMath::Max(1, FMath::RoundToInt(Duration * SampleRate));
		TArray<uint8> Pcm;
		Pcm.SetNumUninitialized(NumSamples * sizeof(int16));
		int16* Out = reinterpret_cast<int16*>(Pcm.GetData());

		FSynth Synth;
		Synth.S ^= static_cast<uint32>(Kind) * 0x9E3779B9u;
		const float Dt = 1.f / static_cast<float>(SampleRate);
		for (int32 i = 0; i < NumSamples; ++i)
		{
			const float S = FMath::Clamp(SampleSfx(Kind, i * Dt, Synth), -1.f, 1.f);
			Out[i] = static_cast<int16>(S * 26000.f);
		}

		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(GetTransientPackage());
		Wave->SetSampleRate(SampleRate);
		Wave->NumChannels = 1;
		Wave->Duration = Duration;
		Wave->bLooping = false;
		Wave->SoundGroup = SOUNDGROUP_Effects;
		Wave->QueueAudio(Pcm.GetData(), Pcm.Num());
		return Wave;
	}

	USoundAttenuation* GetAttenuation()
	{
		static TStrongObjectPtr<USoundAttenuation> Cached;
		if (!Cached.IsValid())
		{
			USoundAttenuation* Attn = NewObject<USoundAttenuation>(GetTransientPackage(), NAME_None, RF_Transient);
			Attn->Attenuation.bAttenuate = true;
			Attn->Attenuation.bSpatialize = true;
			Attn->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
			Attn->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
			Attn->Attenuation.AttenuationShapeExtents = FVector(280.f, 0.f, 0.f);
			Attn->Attenuation.FalloffDistance = 3200.f;
			Cached.Reset(Attn);
		}
		return Cached.Get();
	}
}

void UMobaSfx::Play(
	const UObject* WorldContext,
	USoundBase* Override,
	EMobaSfx Fallback,
	const FVector& Location)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->bIsTearingDown || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	USoundWaveProcedural* Wave = nullptr;
	USoundBase* Sound = IsValid(Override) ? Override : nullptr;
	if (!Sound)
	{
		Wave = MakeWave(Fallback);
		Sound = Wave;
	}
	if (!Sound)
	{
		return;
	}

	float Volume = 0.5f;
	if (const UMobaGameInstance* GI = World->GetGameInstance<UMobaGameInstance>())
	{
		Volume = GI->GetMasterVolume();
	}

	UAudioComponent* Comp = UGameplayStatics::SpawnSoundAtLocation(
		World,
		Sound,
		Location,
		FRotator::ZeroRotator,
		Volume,
		1.f,
		0.f,
		Wave ? GetAttenuation() : nullptr);
	if (Wave && IsValid(Comp))
	{
		Wave->Rename(nullptr, Comp);
	}
}
