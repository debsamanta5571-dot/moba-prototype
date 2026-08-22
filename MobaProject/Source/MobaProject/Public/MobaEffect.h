#pragma once

#include "CoreMinimal.h"
#include "MobaEffect.generated.h"

UENUM(BlueprintType)
enum class EMobaEffectType : uint8
{
	Slow UMETA(ToolTip = "Magnitude is remaining speed (0.5 = half speed)."),
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
