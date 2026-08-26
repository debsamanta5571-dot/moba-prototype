#pragma once

#include "CoreMinimal.h"
#include "MobaGameplayAbility.h"
#include "GA_MobaGroundAoE.generated.h"

class ACharacter;
class AMobaGroundMarker;
class UAnimMontage;
struct FGameplayEventData;

// Hold to place a ground point, release to blast. Server does not re-trace the camera.
UCLASS()
class MOBAPROJECT_API UGA_MobaGroundAoE : public UMobaGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MobaGroundAoE();
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

	virtual void BeginHold(AMobaBaseCharacter* Avatar) const override;
	virtual void ConfirmHold(AMobaBaseCharacter* Avatar) const override;
	virtual void CancelHold(AMobaBaseCharacter* Avatar) const override;

	static FVector TraceGroundAim(const ACharacter* Character, float MaxRange);

	UPROPERTY()
	TObjectPtr<UAnimMontage> GroundTargetMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundAoE")
	TSubclassOf<AMobaGroundMarker> AimRingClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundAoE")
	TSubclassOf<AMobaGroundMarker> BlastClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundAoE")
	float Damage = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundAoE")
	float Radius = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundAoE")
	float MaxRange = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundAoE")
	float BlastLifetime = 0.55f;

protected:
	virtual bool PrepareCast(AMobaBaseCharacter* Character, const FGameplayEventData* TriggerEventData) override;
	virtual void OnCastStarted(AMobaBaseCharacter* Character) override;
	virtual void OnCastNotify(FGameplayEventData Payload) override;
	virtual void StartCastMontage() override;
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual void ExecuteBlast();
	void SpawnBlast(bool bCosmetic);
	void StartBlastWave();
	void StopBlastWave();

	UFUNCTION()
	void FireBlast();

	UFUNCTION()
	void FinishCast();

	UFUNCTION()
	void TickBlastWave();

	FVector PendingBlastLocation = FVector::ZeroVector;
	bool bBlastedThisCast = false;
	int32 BlastLocationRetries = 0;
	FTimerHandle BlastTimer;
	FTimerHandle EndTimer;
	FTimerHandle WaveTimer;

	FVector WaveOrigin = FVector::ZeroVector;
	float WaveMaxRadius = 0.f;
	float WaveDuration = 0.f;
	float WaveDamage = 0.f;
	float WaveStartTime = -100.f;
	bool bWaveHitsTowers = true;
	TSet<TWeakObjectPtr<AActor>> WaveHit;
};
