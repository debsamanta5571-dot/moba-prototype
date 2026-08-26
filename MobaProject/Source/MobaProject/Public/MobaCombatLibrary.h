#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MobaEffect.h"
#include "MobaCombatLibrary.generated.h"

class AActor;
class UMobaStatusComponent;

int32 MobaTeamIdOf(const AActor* Actor);
bool MobaIsEnemy(const AActor* A, const AActor* B);
FLinearColor MobaAttitudeColor(const AActor* Viewer, const AActor* Target);

UCLASS()
class MOBAPROJECT_API UMobaCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static bool ApplyMobaDamage(AActor* Target, float Amount, AActor* Instigator);
	static void AwardKillGold(AActor* Victim, AActor* Killer);
	static void ApplyMobaEffects(
		AActor* HitActor,
		AActor* Instigator,
		const TArray<FMobaEffectSpec>& Effects,
		EMobaEffectTarget Filter);

	static UMobaStatusComponent* GetStatus(AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Moba|Combat")
	static int32 TeamIdOf(const AActor* Actor) { return MobaTeamIdOf(Actor); }

	UFUNCTION(BlueprintPure, Category = "Moba|Combat")
	static bool IsEnemy(const AActor* A, const AActor* B) { return MobaIsEnemy(A, B); }
};
