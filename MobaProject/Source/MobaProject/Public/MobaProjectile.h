#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MobaProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class MOBAPROJECT_API AMobaProjectile : public AActor
{
	GENERATED_BODY()

public:
	AMobaProjectile();

	void InitFlight(const FVector& Direction, float Speed, float InDamage, float Lifetime, bool bCosmetic);

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	float Damage = 25.f;
	bool bCosmetic = false;
	bool bConsumed = false;
};
