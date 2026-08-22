#pragma once

#include "CoreMinimal.h"
#include "MobaGameplayAbility.h"
#include "UGA_MobaMelee.generated.h"

class UAnimMontage;
struct FGameplayEventData;

UCLASS()
class MOBAPROJECT_API UUGA_MobaMelee : public UMobaGameplayAbility
{
	GENERATED_BODY()

public:
	UUGA_MobaMelee();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	TObjectPtr<UAnimMontage> MeleeMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	float Damage = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	float Range = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	float Radius = 60.f;

protected:
	UFUNCTION()
	void OnMeleeHit(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageDone();

	void TryHit();

	bool bHitThisSwing = false;
	bool bEndedThisSwing = false;
};
