#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MobaTower.generated.h"

class AMobaProjectile;
class UAbilitySystemComponent;
class UBoxComponent;
class UMobaAttributeSet;
class USceneComponent;
class UStaticMeshComponent;
class UWidgetComponent;

UCLASS(Blueprintable)
class MOBAPROJECT_API AMobaTower : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMobaTower();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	int32 GetTeamId() const { return TeamID; }

	UFUNCTION(BlueprintPure, Category = "Moba")
	float GetHealth() const;

protected:
	virtual void BeginPlay() override;

	void Fire();
	AActor* FindClosestEnemy() const;
	void HandleDeath();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UWidgetComponent> HealthWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UBoxComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UMobaAttributeSet> AttributeSet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Moba")
	int32 TeamID = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	TSubclassOf<AMobaProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float Range = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float FireInterval = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float Damage = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float ProjectileSpeed = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float ProjectileLifetime = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float MaxHealth = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	FVector FireOffset = FVector(0.f, 0.f, 200.f);

	FTimerHandle FireTimer;
	bool bDead = false;
};
