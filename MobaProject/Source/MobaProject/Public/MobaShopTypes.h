#pragma once

#include "CoreMinimal.h"
#include "MobaShopTypes.generated.h"

UENUM(BlueprintType)
enum class EMobaShopStat : uint8
{
	Damage,
	Energy,
	CooldownReduction,
	Health,
	DamageResistance,
	MoveSpeed
};

USTRUCT(BlueprintType)
struct FMobaShopOffer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	EMobaShopStat Stat = EMobaShopStat::Damage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float Magnitude = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float Cost = 100.f;
};
