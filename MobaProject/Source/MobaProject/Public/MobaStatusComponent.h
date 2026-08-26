#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "MobaEffect.h"
#include "MobaStatusComponent.generated.h"

UCLASS(ClassGroup = (Moba), meta = (BlueprintSpawnableComponent))
class MOBAPROJECT_API UMobaStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMobaStatusComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void ApplySpec(const FMobaEffectSpec& Spec);
	void RefreshMoveSpeed();
	void ClearStun();
	void RecalcSlow();
	void ClearSlow();
	void ClearHaste();
	void ClearAll();
	void SanitizeTimedState();

	bool IsStunned() const { return bStunned; }
	bool IsSlowed() const { return SlowMul < 0.99f; }
	float GetSlowMul() const { return SlowMul; }
	float GetHasteMul() const { return HasteMul; }

protected:
	float GetServerTimeSeconds() const;
	bool IsTimerExpired(float UntilTime) const;
	void SetOwnerStatusTag(FName TagName, bool bEnabled);
	void NotifyOwnerStunned();
	bool IsOwnerDead() const;
	float GetOwnerCastSpeedMul() const;

	UFUNCTION()
	void OnRep_MoveStatus();

	UPROPERTY(ReplicatedUsing = OnRep_MoveStatus)
	bool bStunned = false;

	UPROPERTY(ReplicatedUsing = OnRep_MoveStatus)
	float SlowMul = 1.f;

	UPROPERTY(ReplicatedUsing = OnRep_MoveStatus)
	float HasteMul = 1.f;

	UPROPERTY(ReplicatedUsing = OnRep_MoveStatus)
	float StunUntilTime = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_MoveStatus)
	float SlowUntilTime = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_MoveStatus)
	float HasteUntilTime = 0.f;

	TArray<FMobaSlowEntry> SlowStack;
	FTimerHandle StunTimer;
	FTimerHandle SlowTimer;
	FTimerHandle HasteTimer;
};
