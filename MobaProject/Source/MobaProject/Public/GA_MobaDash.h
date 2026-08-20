#pragma once

#include "CoreMinimal.h"
#include "MobaGameplayAbility.h"
#include "GA_MobaDash.generated.h"

class ACharacter;
class UAnimMontage;
struct FGameplayEventData;

UCLASS()
class MOBAPROJECT_API UGA_MobaDash : public UMobaGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MobaDash();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	TObjectPtr<UAnimMontage> DashMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	float DashStrength = 3500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	float DashDuration = 0.2f;

protected:
	UFUNCTION()
	void OnDashFinished();

	static FVector DirectionFromEvent(const FGameplayEventData* EventData, const ACharacter* FallbackCharacter);
};
