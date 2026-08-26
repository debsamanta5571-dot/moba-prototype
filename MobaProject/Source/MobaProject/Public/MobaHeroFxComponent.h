#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "MobaHeroFxComponent.generated.h"

UCLASS(ClassGroup = (Moba), meta = (BlueprintSpawnableComponent))
class MOBAPROJECT_API UMobaHeroFxComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMobaHeroFxComponent();

	void TickFx();
	void StartGroundAimDebug(float Radius, float MaxRange);
	void StopGroundAimDebug();
	void PlayGroundBlastDebug(FVector Location, float Radius, float Lifetime);
	void PlayFireRingDebug(float Radius, float Lifetime);
	void PlayBeamDebug(FVector Start, FVector End, float Radius, float Lifetime);
	void StopBeamDebug();
	void SpawnFloatingNumber(FVector Location, float Amount, bool bGold);

protected:
	bool bGroundAiming = false;
	float GroundAimRadius = 250.f;
	float GroundAimMaxRange = 1400.f;
	FVector GroundBlastLoc = FVector::ZeroVector;
	float GroundBlastRadius = 250.f;
	float GroundBlastDuration = 0.55f;
	float GroundBlastStartTime = -100.f;
	float FireRingRadius = 260.f;
	float FireRingDuration = 0.7f;
	float FireRingStartTime = -100.f;
	FVector BeamStartLoc = FVector::ZeroVector;
	FVector BeamEndLoc = FVector::ZeroVector;
	float BeamRadius = 42.f;
	float BeamDuration = 0.2f;
	float BeamStartTime = -100.f;
};
