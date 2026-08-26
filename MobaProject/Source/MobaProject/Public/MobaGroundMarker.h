#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MobaGroundMarker.generated.h"

class AActor;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UWorld;

UCLASS(Blueprintable)
class MOBAPROJECT_API AMobaGroundMarker : public AActor
{
	GENERATED_BODY()

public:
	AMobaGroundMarker();

	void InitAsAimRing(float Radius, float MaxRange);
	void InitAsBlast(float Radius, float Lifetime, bool bCosmetic);
	static void DestroyAllFor(UWorld* World, const AActor* OwnerOrInstigator);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsNetRelevantFor(
		const AActor* RealViewer,
		const AActor* ViewTarget,
		const FVector& SrcLocation) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	TObjectPtr<UStaticMesh> DisplayMesh;

	void ApplyDisplayMesh();
	void SetRadiusScale(float Radius, float HeightScale);
	void HideAllVisuals();
	void ShowAllVisuals();

	UPROPERTY(Replicated)
	float TargetRadius = 250.f;

	UPROPERTY(Replicated)
	float BlastDuration = 0.55f;

	UPROPERTY(Replicated)
	bool bExpanding = false;

	float AimMaxRange = 1400.f;
	float ElapsedTime = 0.f;
	bool bAiming = false;
	bool bCosmetic = false;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BlastMid;
};
