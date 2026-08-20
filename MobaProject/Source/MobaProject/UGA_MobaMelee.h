#pragma once

#include "Abilities/GameplayAbility.h"
#include "CoreMinimal.h"
#include "UGA_MobaMelee.generated.h"

UCLASS()
class MOBAPROJECT_API UUGA_MobaMelee : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UUGA_MobaMelee();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	float Damage = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	float Range = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	float Radius = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	float Cooldown = 1.f;
};
