#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MobaSfx.generated.h"

class USoundBase;

UENUM(BlueprintType)
enum class EMobaSfx : uint8
{
	None = 0,
	MeleeCast,
	MeleeHit,
	Dash,
	SkillshotFire,
	GroundBlast,
	ProjectileDestroy,
	MinionAttack,
	MinionHit,
	TowerFire
};

UCLASS()
class MOBAPROJECT_API UMobaSfx : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static void Play(
		const UObject* WorldContext,
		USoundBase* Override,
		EMobaSfx Fallback,
		const FVector& Location);

	static void ToggleMute(const UObject* WorldContext);
	static bool IsMuted(const UObject* WorldContext);
};
