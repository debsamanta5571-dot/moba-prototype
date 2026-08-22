#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MobaTower.generated.h"

class AMobaProjectile;
class UAbilitySystemComponent;
class UBoxComponent;
class UPrimitiveComponent;
class UMobaAttributeSet;
class USceneComponent;
class USoundBase;
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
	const UPrimitiveComponent* GetBlockingCollision() const;

	UFUNCTION(BlueprintPure, Category = "Moba")
	float GetHealth() const;

	void HandleDeath();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	void ConfigureCollision();
	void PlaceHealthWidget();
	void Fire();
	AActor* FindClosestEnemy() const;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFireSfx();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UWidgetComponent> HealthWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UBoxComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<USceneComponent> FirePoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UMobaAttributeSet> AttributeSet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Moba",
		meta = (ToolTip = "Must differ from the attacker. First player is team 1, so enemy towers should be 2."))
	int32 TeamID = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	TSubclassOf<AMobaProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float Range = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float FireInterval = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float ProjectileSpeed = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float ProjectileLifetime = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|SFX")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float MaxHealth = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float Damage = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float DamageModifier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float DamageResistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float GoldOnKill = 0.f;

	FTimerHandle FireTimer;
	bool bDead = false;
};
