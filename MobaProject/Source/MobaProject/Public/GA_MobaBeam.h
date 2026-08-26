#pragma once

#include "CoreMinimal.h"
#include "MobaGameplayAbility.h"
#include "GA_MobaBeam.generated.h"

class AMobaBaseCharacter;

UCLASS()
class MOBAPROJECT_API UGA_MobaBeam : public UMobaGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MobaBeam();

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
	float Duration = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
	float Range = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
	float Radius = 42.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
	float DamagePerTick = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
	float TickInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam",
		meta = (ToolTip = "Mesh socket to spawn from. Leave none to use the character location."))
	FName SpawnSocket;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam",
		meta = (ToolTip = "Socket local offset if a socket is set. Otherwise character space; X forward, Y right, Z up."))
	FVector SpawnOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
	float TurnSpeed = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam", meta = (ClampMin = "1.0", ClampMax = "85.0"))
	float MaxAimPitch = 70.f;

protected:
	virtual bool PrepareCast(AMobaBaseCharacter* Character, const FGameplayEventData* TriggerEventData) override;
	virtual void OnCastNotify(FGameplayEventData Payload) override;
	virtual float GetPlantDuration() const override { return Duration; }

	UFUNCTION()
	void TickBeam();

	UFUNCTION()
	void FinishBeam();

	void ApplyTickDamage(AMobaBaseCharacter* Character, const FVector& Start, const FVector& End);
	void StopBeamTimers();
	void StopBeamVfx(AMobaBaseCharacter* Character);

	FTimerHandle TickTimer;
	FTimerHandle EndTimer;
	TSet<TWeakObjectPtr<AActor>> HitThisBeam;
	bool bEndedThisCast = false;
};
