#pragma once

#include "CoreMinimal.h"
#include "MobaGameplayAbility.h"
#include "GA_MobaGroundTarget.generated.h"

class ACharacter;
class AMobaGroundMarker;
class UAnimMontage;
struct FGameplayEventData;

UCLASS()
class MOBAPROJECT_API UGA_MobaGroundTarget : public UMobaGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MobaGroundTarget();
	virtual void PostInitProperties() override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void BeginHold(AMobaBaseCharacter* Avatar) const override;
	virtual void ConfirmHold(AMobaBaseCharacter* Avatar) const override;
	virtual void CancelHold(AMobaBaseCharacter* Avatar) const override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	static FVector TraceGroundAim(const ACharacter* Character, float MaxRange);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundTarget")
	TObjectPtr<UAnimMontage> GroundTargetMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundTarget")
	TSubclassOf<AMobaGroundMarker> AimRingClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundTarget")
	TSubclassOf<AMobaGroundMarker> BlastClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundTarget")
	float Damage = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundTarget")
	float Radius = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundTarget")
	float MaxRange = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundTarget")
	float BlastLifetime = 0.55f;

protected:
	UFUNCTION()
	void FireBlast();

	UFUNCTION()
	void OnMontageDone();

	void SpawnBlast(bool bCosmetic);
	void ApplyBlastDamage();

	FVector PendingBlastLocation = FVector::ZeroVector;
	bool bBlastedThisCast = false;
	bool bEndedThisCast = false;
	FTimerHandle CastFailsafeTimer;
	FTimerHandle BlastTimer;
};
