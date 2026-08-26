#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "MobaBeamComponent.generated.h"

class AMobaBaseCharacter;
class UMobaHeroFxComponent;

// Live beam aim + replication. DrawDebug lives on HeroFx; damage ticks live on UGA_MobaBeam.
UCLASS(ClassGroup = (Moba), meta = (BlueprintSpawnableComponent))
class MOBAPROJECT_API UMobaBeamComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMobaBeamComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Start(FName Socket, const FVector& Offset, float Range, float Radius, float Lifetime, float TurnSpeed = 14.f, float MaxPitch = 70.f);
	void Stop();
	bool IsActive() const { return bBeamActive; }
	FVector GetStart() const { return SmoothStart; }
	FVector GetEnd() const { return SmoothEnd; }

protected:
	AMobaBaseCharacter* GetHero() const;
	UMobaHeroFxComponent* GetFx() const;
	FRotator GetAimRotator() const;
	FVector ComputeFirePoint(const FRotator& Aim) const;
	FVector ClipEnd(const FVector& Start, const FVector& WantedEnd) const;
	void TickBeam(float DeltaSeconds);

	bool bBeamActive = false;
	bool bSmoothed = false;
	FName Socket;
	FVector Offset = FVector::ZeroVector;
	float Range = 1400.f;
	float Radius = 42.f;
	float TurnSpeed = 14.f;
	float MaxPitch = 70.f;
	float EndTime = 0.f;
	FVector SmoothStart = FVector::ZeroVector;
	FVector SmoothEnd = FVector::ZeroVector;
	FVector SmoothDir = FVector::ForwardVector;

	UPROPERTY(Replicated)
	bool bRepBeaming = false;

	UPROPERTY(Replicated)
	FVector RepStart = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FVector RepDir = FVector::ForwardVector;

	UPROPERTY(Replicated)
	float RepRange = 1400.f;

	UPROPERTY(Replicated)
	float RepRadius = 42.f;
};
