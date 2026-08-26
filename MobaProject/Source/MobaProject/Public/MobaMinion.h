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
class UMobaStatusComponent;
class UWidgetComponent;

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
	bool IsStunned() const;
	bool IsSlowed() const;
	bool IsDead() const { return bDead; }
	UMobaStatusComponent* GetStatus() const { return Status; }
	AMobaTower* GetGoalTower() const { return GoalTower; }
	float GetAggroRange() const { return AggroRange; }
	float GetAttackRange() const { return AttackRange; }
	float GetAttackInterval() const { return AttackInterval; }
	float GetLastAttackTime() const { return LastAttackTime; }
	float GetPlayerChaseGiveUpSeconds() const { return PlayerChaseGiveUpSeconds; }
	float GetServerTimeSeconds() const;
	AActor* GetCombatTarget() const;
	AMobaTower* FindEnemyTower() const;
	void NotifyDamagedBy(AActor* DamageCauser);
	void NotePlayerDamageFrom(AMobaBaseCharacter* Player);
	AMobaBaseCharacter* GetPlayerKillCredit() const;
	void ClearPlayerKillCredit();
	void HandleDeath(AActor* Killer = nullptr);
	void RefreshMoveSpeed();
	void TryAttack(AActor* Target);
	void FaceActor(AActor* Target);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;

	void DealAttackDamage();
	float DistanceToAttackTarget(const AActor* Target) const;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMobaStatusComponent> Status;

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

	void SanitizeTimedState();
	bool IsTimerExpired(float UntilTime) const;

	float LastAttackTime = -100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bDead = false;

	FTimerHandle PlayerKillCreditTimer;
	TWeakObjectPtr<AMobaBaseCharacter> LastPlayerDamager;
	float PlayerKillCreditUntilTime = 0.f;
	TWeakObjectPtr<AActor> PendingAttackTarget;
	FTimerHandle AttackHitTimer;
};
