#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MobaEffect.h"
#include "MobaProjectile.generated.h"

class UProjectileMovementComponent;
class USoundBase;
class USphereComponent;
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
		bool bHideVisuals = false);

	virtual void BeginPlay() override;
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	float Damage = 25.f;
	bool bCosmetic = false;
	bool bHideVisuals = false;
	bool bConsumed = false;
	TArray<FMobaEffectSpec> HitEffects;
};
