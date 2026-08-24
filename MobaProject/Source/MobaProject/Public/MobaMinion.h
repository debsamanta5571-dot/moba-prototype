#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MobaEffect.h"
#include "MobaMinion.generated.h"

class AMobaBaseCharacter;
class AMobaTower;
class UAbilitySystemComponent;
class UAnimationAsset;
class UAnimMontage;
class UMobaAttributeSet;
class UWidgetComponent;

int32 MobaTeamIdOf(const AActor* Actor);
bool MobaIsEnemy(const AActor* A, const AActor* B);
FLinearColor MobaAttitudeColor(const AActor* Viewer, const AActor* Target);

UCLASS(Blueprintable)
class MOBAPROJECT_API AMobaMinion : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMobaMinion();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	int32 GetTeamId() const { return TeamID; }
	void SetTeamId(int32 InTeam);
	void SetGoalTower(AMobaTower* Tower);

	UFUNCTION(BlueprintPure, Category = "Moba")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Moba")
	float GetGoldOnKill() const;

	void ApplyStatus(const FMobaEffectSpec& Spec);
	bool IsStunned() const { return bStunned; }
	bool IsSlowed() const { return SlowMul < 0.99f; }
	void NotifyDamagedBy(AActor* DamageCauser);
	void NotePlayerDamageFrom(AMobaBaseCharacter* Player);
	AMobaBaseCharacter* GetPlayerKillCredit() const;
	void ClearPlayerKillCredit();
	void HandleDeath(AActor* Killer = nullptr);
	void RefreshMoveSpeed();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;

	void Think();
	void Engage(AActor* Target);
	void Chase(AActor* Target);
	void TryAttack(AActor* Target);
	void FaceActor(AActor* Target);
	void DealAttackDamage();
	AActor* FindClosestHero() const;
	AActor* FindClosestMinion() const;
	AMobaTower* FindEnemyTower() const;
	void MoveToward(const FVector& Dest);
	float DistanceToAttackTarget(const AActor* Target) const;
	FVector GetAttackApproachPoint(const AActor* Target) const;
	bool IsValidCombatTarget(const AActor* Target) const;
	bool ShouldKeepTarget(const AActor* Target) const;
	AActor* AcquireNewTarget();
	int32 CountAlliedMinionsTargeting(const AActor* Target) const;
	void SetCombatTarget(AActor* Target);
	void RefreshPlayerChase(AActor* Target);
	bool HasFailedPlayerChase() const;
	void GiveUpPlayerChase();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayAttack();

	void ApplyDeathPresentation();
	void PlayDeathAnimation();

	UFUNCTION()
	void OnRep_Dead();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UWidgetComponent> HealthWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UMobaAttributeSet> AttributeSet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Moba")
	int32 TeamID = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	TObjectPtr<AMobaTower> GoalTower;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float AggroRange = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba", meta = (ClampMin = "0.0"))
	float PlayerChaseGiveUpSeconds = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float AttackRange = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float AttackInterval = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	TObjectPtr<UAnimationAsset> DeathAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float AttackHitDelay = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float MaxHealth = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float AttackDamage = 24.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float GoldOnKill = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba", meta = (ClampMin = "0.0"))
	float PlayerKillCreditSeconds = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float MoveSpeed = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float DamageModifier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float DamageResistance = 0.f;

	void ClearStun();
	void ClearSlow();
	void ClearHaste();
	void SanitizeTimedState();
	float GetServerTimeSeconds() const;
	bool IsTimerExpired(float UntilTime) const;

	float LastAttackTime = -100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bDead = false;

	UPROPERTY(Replicated)
	bool bStunned = false;

	UPROPERTY(Replicated)
	float SlowMul = 1.f;
	float HasteMul = 1.f;

	UPROPERTY(Replicated)
	float StunUntilTime = 0.f;

	UPROPERTY(Replicated)
	float SlowUntilTime = 0.f;

	UPROPERTY(Replicated)
	float HasteUntilTime = 0.f;
	FTimerHandle StunTimer;
	FTimerHandle SlowTimer;
	FTimerHandle HasteTimer;
	FTimerHandle PlayerKillCreditTimer;
	TWeakObjectPtr<AMobaBaseCharacter> LastPlayerDamager;
	float PlayerKillCreditUntilTime = 0.f;
	TWeakObjectPtr<AActor> PendingAttackTarget;
	TWeakObjectPtr<AActor> CombatTarget;
	TWeakObjectPtr<AMobaBaseCharacter> ChasedPlayer;
	TWeakObjectPtr<AMobaBaseCharacter> LeashIgnoredPlayer;
	float ChasePlayerStartTime = 0.f;
	bool bDamagedChasedPlayer = false;
	FTimerHandle AttackHitTimer;
};
