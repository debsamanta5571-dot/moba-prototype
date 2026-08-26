#pragma once

#include "CoreMinimal.h"
#include "MobaEffect.generated.h"

UENUM(BlueprintType)
enum class EMobaEffectType : uint8
{
	Slow UMETA(ToolTip = "Magnitude is slow amount (0.2 = 20% slower). Values over 1 are percent (20 = 20%). Stacks multiplicatively."),
	Stun UMETA(ToolTip = "Magnitude unused. Duration is stun time."),
	Heal UMETA(ToolTip = "Magnitude is health restored."),
	MoveSpeed UMETA(ToolTip = "Magnitude is speed multiplier (1.5 = 50% faster).")
};

UENUM(BlueprintType)
enum class EMobaEffectTarget : uint8
{
	HitActor,
	Self
};

USTRUCT(BlueprintType)
struct FMobaEffectSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	EMobaEffectType Type = EMobaEffectType::Slow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	EMobaEffectTarget Target = EMobaEffectTarget::HitActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float Magnitude = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float Duration = 1.5f;
};

struct FMobaSlowEntry
{
	float Amount = 0.f;
	float EndTime = 0.f;
};

namespace MobaSlow
{
	inline float NormalizeAmount(float Magnitude)
	{
		float Amount = Magnitude;
		if (Amount > 1.f)
		{
			Amount *= 0.01f;
		}
		return FMath::Clamp(Amount, 0.f, 1.f);
	}

	inline void Add(TArray<FMobaSlowEntry>& Stack, float Magnitude, float Duration, float Now)
	{
		const float Amount = NormalizeAmount(Magnitude);
		if (Amount <= 0.f || Duration <= 0.f)
		{
			return;
		}
		FMobaSlowEntry Entry;
		Entry.Amount = Amount;
		Entry.EndTime = Now + Duration;
		Stack.Add(Entry);
	}

	inline void Prune(TArray<FMobaSlowEntry>& Stack, float Now)
	{
		for (int32 i = Stack.Num() - 1; i >= 0; --i)
		{
			if (Stack[i].EndTime <= Now)
			{
				Stack.RemoveAtSwap(i);
			}
		}
	}

	inline float RemainingMul(const TArray<FMobaSlowEntry>& Stack, float Now)
	{
		float Mul = 1.f;
		for (const FMobaSlowEntry& Entry : Stack)
		{
			if (Entry.EndTime > Now)
			{
				Mul *= 1.f - FMath::Clamp(Entry.Amount, 0.f, 1.f);
			}
		}
		return FMath::Clamp(Mul, 0.f, 1.f);
	}

	inline float NextEndTime(const TArray<FMobaSlowEntry>& Stack, float Now)
	{
		float Next = 0.f;
		for (const FMobaSlowEntry& Entry : Stack)
		{
			if (Entry.EndTime > Now && (Next <= 0.f || Entry.EndTime < Next))
			{
				Next = Entry.EndTime;
			}
		}
		return Next;
	}

	inline float LastEndTime(const TArray<FMobaSlowEntry>& Stack, float Now)
	{
		float Last = 0.f;
		for (const FMobaSlowEntry& Entry : Stack)
		{
			if (Entry.EndTime > Now)
			{
				Last = FMath::Max(Last, Entry.EndTime);
			}
		}
		return Last;
	}
}
