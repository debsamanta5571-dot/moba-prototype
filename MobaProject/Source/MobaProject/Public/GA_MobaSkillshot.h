#pragma once

#include "CoreMinimal.h"
#include "MobaGameplayAbility.h"
#include "GA_MobaSkillshot.generated.h"

class AMobaBaseCharacter;
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

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skillshot",
		meta = (ToolTip = "Mesh socket to spawn from. Leave none to use the character location."))
	FName SpawnSocket;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skillshot",
		meta = (ToolTip = "Added after the socket or character. X is forward, Y is right, Z is up."))
	FVector SpawnOffset = FVector(80.f, 0.f, 40.f);

protected:
	UFUNCTION()
	void FireShot();

	UFUNCTION()
	void OnAnimNotify(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageDone();

	void SpawnBolt(bool bCosmetic, bool bHideVisuals = false);
	FVector GetSpawnLocation(AMobaBaseCharacter* Character) const;

	bool bFiredThisCast = false;
	bool bEndedThisCast = false;
};
