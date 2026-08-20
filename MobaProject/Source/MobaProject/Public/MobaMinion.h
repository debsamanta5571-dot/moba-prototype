#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MobaMinion.generated.h"

class AMobaTower;
class UAbilitySystemComponent;
class UAnimMontage;
class UMobaAttributeSet;
class UWidgetComponent;

int32 MobaTeamIdOf(const AActor* Actor);

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

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void Think();
	void Chase(AActor* Target);
	void TryAttack(AActor* Target);
	void FaceActor(AActor* Target);
	void DealAttackDamage();
	AActor* FindClosestHero() const;
	AMobaTower* FindEnemyTower() const;
	void MoveToward(const FVector& Dest);
	void HandleDeath();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayAttack();

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float AttackRange = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float AttackDamage = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float AttackInterval = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float AttackHitDelay = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float MaxHealth = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float MoveSpeed = 320.f;

	float LastAttackTime = -100.f;
	bool bDead = false;
	TWeakObjectPtr<AActor> PendingAttackTarget;
	FTimerHandle AttackHitTimer;
};
