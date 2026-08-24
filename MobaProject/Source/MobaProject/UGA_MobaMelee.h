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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "1", ToolTip = "How many units this swing can hit, closest first."))
	int32 MaxTargets = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee",
		meta = (ToolTip = "Draw a fire ring on the ground at slash reach. Use for flamestrike."))
	bool bShowRangeRing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (EditCondition = "bShowRangeRing"))
	float RangeRingLifetime = 0.7f;

protected:
	UFUNCTION()
	void OnMeleeHit(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageDone();

	void TryHit();
	void ShowRangeRing();

	bool bHitThisSwing = false;
	bool bEndedThisSwing = false;
};
