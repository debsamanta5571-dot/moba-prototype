#pragma once

#include "CoreMinimal.h"
#include "MobaGameplayAbility.h"
#include "GA_MobaSkillshot.generated.h"

class AMobaProjectile;
class UAnimMontage;
struct FGameplayEventData;

UCLASS()
class MOBAPROJECT_API UGA_MobaSkillshot : public UMobaGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MobaSkillshot();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skillshot")
	TObjectPtr<UAnimMontage> SkillshotMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skillshot")
	TSubclassOf<AMobaProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skillshot")
	float Damage = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skillshot")
	float Speed = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skillshot")
	float Lifetime = 2.f;

protected:
	UFUNCTION()
	void OnSkillshotFire(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageDone();

	void SpawnBolt(bool bCosmetic);

	bool bFiredThisCast = false;
	bool bEndedThisCast = false;
};
