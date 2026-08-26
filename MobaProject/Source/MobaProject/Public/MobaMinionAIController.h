#pragma once

#include "AIController.h"
#include "CoreMinimal.h"
#include "MobaMinionAIController.generated.h"

class AMobaBaseCharacter;
class AMobaMinion;
class AMobaTower;

// Lane brain. The pawn still owns mesh, GAS, attack montage, and death.
UCLASS()
class MOBAPROJECT_API AMobaMinionAIController : public AAIController
{
	GENERATED_BODY()

public:
	AMobaMinionAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

	AActor* GetCombatTarget() const { return CombatTarget.Get(); }
	void SetCombatTarget(AActor* Target);
	void NotifyDamagedBy(AActor* DamageCauser);
	void NotifyAttackHit(AActor* Target);
	void ClearCombat();

protected:
	AMobaMinion* GetMinion() const;
	void Think();
	void Engage(AActor* Target);
	void Chase(AActor* Target);
	AActor* FindClosestHero() const;
	AActor* FindClosestMinion() const;
	int32 CountAlliedMinionsTargeting(const AActor* Target) const;
	void MoveToward(const FVector& Dest);
	float DistanceToAttackTarget(const AActor* Target) const;
	FVector GetAttackApproachPoint(const AActor* Target) const;
	bool IsValidCombatTarget(const AActor* Target) const;
	bool ShouldKeepTarget(const AActor* Target) const;
	AActor* AcquireNewTarget();
	void RefreshPlayerChase(AActor* Target);
	bool HasFailedPlayerChase() const;
	void GiveUpPlayerChase();

	TWeakObjectPtr<AActor> CombatTarget;
	TWeakObjectPtr<AMobaBaseCharacter> ChasedPlayer;
	TWeakObjectPtr<AMobaBaseCharacter> LeashIgnoredPlayer;
	float ChasePlayerStartTime = 0.f;
	bool bDamagedChasedPlayer = false;
};
