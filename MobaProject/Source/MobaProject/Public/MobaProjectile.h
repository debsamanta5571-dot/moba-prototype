#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MobaEffect.h"
#include "MobaSfx.h"
#include "MobaProjectile.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProjectileMovementComponent;
class USoundBase;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class MOBAPROJECT_API AMobaProjectile : public AActor
{
	GENERATED_BODY()

public:
	AMobaProjectile();

	void InitFlight(
		const FVector& Direction,
		float Speed,
		float InDamage,
		float Lifetime,
		bool bCosmetic,
		const TArray<FMobaEffectSpec>& InHitEffects = TArray<FMobaEffectSpec>(),
		bool bHideVisuals = false,
		bool bInCanDamageTowers = true);

	void InitHoming(AActor* Target, float Speed, float InDamage, float Lifetime);
	void SetImpactRadius(float Radius);
	void SetExplodeAtZ(float Z);
	void ApplyLook();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void LifeSpanExpired() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool IsNetRelevantFor(
		const AActor* RealViewer,
		const AActor* ViewTarget,
		const FVector& SrcLocation) const override;

protected:
	UFUNCTION()
	void OnSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnHit(
		UPrimitiveComponent* HitComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	UFUNCTION()
	void OnStop(const FHitResult& ImpactResult);

	void ConsumeAndDestroy(AActor* DamageTarget);
	void SpawnDestroyVfx();
	void SetupCollision();
	void HideAllVisuals();
	void ShowAllVisuals();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	TSubclassOf<AActor> DestroyVfxClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float DestroyVfxLife = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|SFX")
	TObjectPtr<USoundBase> DestroySound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|SFX")
	EMobaSfx DefaultDestroySfx = EMobaSfx::ProjectileDestroy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Look")
	TObjectPtr<UMaterialInterface> BoltMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Look")
	TObjectPtr<UStaticMesh> BoltMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Look")
	FLinearColor BoltColor = FLinearColor(0.15f, 0.75f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Look", meta = (ClampMin = "0.1"))
	float VisualScale = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Look", meta = (ClampMin = "1.0"))
	float CollisionRadius = 16.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	float Damage = 25.f;
	float ImpactRadius = 0.f;
	float ExplodeAtZ = -1.e9f;
	bool bCosmetic = false;
	bool bCanDamageTowers = true;
	bool bHideVisuals = false;
	bool bConsumed = false;
	float HomingHitRadius = 120.f;
	TWeakObjectPtr<AActor> HomingTarget;
	TArray<FMobaEffectSpec> HitEffects;
	TObjectPtr<UMaterialInstanceDynamic> BoltMid;
};
