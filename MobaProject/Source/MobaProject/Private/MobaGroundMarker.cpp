#include "MobaGroundMarker.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GA_MobaGroundTarget.h"
#include "MobaBaseCharacter.h"
#include "Net/UnrealNetwork.h"

AMobaGroundMarker::AMobaGroundMarker()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetMobility(EComponentMobility::Movable);
}

void AMobaGroundMarker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyDisplayMesh();
}

void AMobaGroundMarker::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMobaGroundMarker, TargetRadius);
	DOREPLIFETIME(AMobaGroundMarker, BlastDuration);
	DOREPLIFETIME(AMobaGroundMarker, bExpanding);
}

void AMobaGroundMarker::ApplyDisplayMesh()
{
	if (DisplayMesh)
	{
		Mesh->SetStaticMesh(DisplayMesh);
	}
}

void AMobaGroundMarker::SetRadiusScale(float Radius, float HeightScale)
{
	const float Scale = FMath::Max(Radius, 1.f) / 50.f;
	Mesh->SetWorldScale3D(FVector(Scale, Scale, Scale * HeightScale));
}

void AMobaGroundMarker::InitAsAimRing(float Radius, float MaxRange)
{
	ApplyDisplayMesh();
	bAiming = true;
	bExpanding = false;
	bCosmetic = true;
	TargetRadius = Radius;
	AimMaxRange = MaxRange;
	SetReplicates(false);
	SetRadiusScale(Radius, 0.08f);
}

void AMobaGroundMarker::InitAsBlast(float Radius, float Lifetime, bool bInCosmetic)
{
	ApplyDisplayMesh();
	bAiming = false;
	bExpanding = true;
	bCosmetic = bInCosmetic;
	TargetRadius = Radius;
	BlastDuration = FMath::Max(Lifetime, 0.05f);
	SetRadiusScale(1.f, 1.f);

	if (bCosmetic)
	{
		SetReplicates(false);
	}
	else
	{
		Mesh->SetOwnerNoSee(true);
		ForceNetUpdate();
	}

	SetLifeSpan(BlastDuration);
}

void AMobaGroundMarker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bAiming)
	{
		if (const AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(GetOwner()))
		{
			SetActorLocation(UGA_MobaGroundTarget::TraceGroundAim(Hero, AimMaxRange));
		}
		SetRadiusScale(TargetRadius, 0.08f);
	}
	else if (bExpanding)
	{
		const float Alpha = FMath::Clamp(GetGameTimeSinceCreation() / BlastDuration, 0.f, 1.f);
		SetRadiusScale(TargetRadius * Alpha, 1.f);
	}

	if (!Mesh->GetStaticMesh())
	{
		if (bExpanding && !bCosmetic)
		{
			if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
			{
				if (OwnerPawn->IsLocallyControlled())
				{
					return;
				}
			}
		}

		const float DrawRadius = bExpanding
			? TargetRadius * FMath::Clamp(GetGameTimeSinceCreation() / BlastDuration, 0.f, 1.f)
			: TargetRadius;
		DrawDebugSphere(
			GetWorld(),
			GetActorLocation(),
			DrawRadius,
			16,
			bAiming ? FColor::Cyan : FColor::Orange,
			false,
			-1.f,
			0,
			2.f);
	}
}

bool AMobaGroundMarker::IsNetRelevantFor(
	const AActor* RealViewer,
	const AActor* ViewTarget,
	const FVector& SrcLocation) const
{
	if (bCosmetic)
	{
		return false;
	}

	const APawn* InstigatorPawn = GetInstigator();
	if (InstigatorPawn && (ViewTarget == InstigatorPawn || RealViewer == InstigatorPawn->GetController()))
	{
		return false;
	}

	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}
