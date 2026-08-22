#include "MobaGroundMarker.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GA_MobaGroundTarget.h"
#include "GameFramework/Pawn.h"
#include "MobaBaseCharacter.h"
#include "Net/UnrealNetwork.h"

AMobaGroundMarker::AMobaGroundMarker()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetIsReplicated(false);
}

void AMobaGroundMarker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
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
}

void AMobaGroundMarker::SetRadiusScale(float Radius, float HeightScale)
{
	const float Scale = FMath::Max(Radius, 1.f) / 50.f;
	const FVector NewScale(Scale, Scale, Scale * HeightScale);
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Comp : Meshes)
	{
		if (Comp)
		{
			Comp->SetWorldScale3D(NewScale);
		}
	}
}

void AMobaGroundMarker::InitAsAimRing(float Radius, float MaxRange)
{
	bAiming = true;
	bExpanding = false;
	bCosmetic = true;
	TargetRadius = Radius;
	AimMaxRange = MaxRange;
	SetReplicates(false);
	HideAllVisuals();
}

void AMobaGroundMarker::InitAsBlast(float Radius, float Lifetime, bool bInCosmetic)
{
	bAiming = false;
	bExpanding = true;
	bCosmetic = true;
	TargetRadius = Radius;
	BlastDuration = FMath::Max(Lifetime, 0.05f);
	SetReplicates(false);
	HideAllVisuals();
	SetLifeSpan(BlastDuration);
}

void AMobaGroundMarker::DestroyAllFor(UWorld* World, const AActor* OwnerOrInstigator)
{
	if (!World || !OwnerOrInstigator)
	{
		return;
	}

	TArray<AMobaGroundMarker*> ToDestroy;
	for (TActorIterator<AMobaGroundMarker> It(World); It; ++It)
	{
		AMobaGroundMarker* Marker = *It;
		if (Marker && (Marker->GetOwner() == OwnerOrInstigator || Marker->GetInstigator() == OwnerOrInstigator))
		{
			ToDestroy.Add(Marker);
		}
	}
	for (AMobaGroundMarker* Marker : ToDestroy)
	{
		Marker->Destroy();
	}
}

void AMobaGroundMarker::BeginPlay()
{
	Super::BeginPlay();
}

void AMobaGroundMarker::HideAllVisuals()
{
	TArray<UPrimitiveComponent*> Prims;
	GetComponents<UPrimitiveComponent>(Prims);
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (Prim)
		{
			Prim->SetVisibility(false, true);
			Prim->SetHiddenInGame(true, true);
		}
	}
}

void AMobaGroundMarker::ShowAllVisuals()
{
	TArray<UPrimitiveComponent*> Prims;
	GetComponents<UPrimitiveComponent>(Prims);
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (Prim)
		{
			Prim->SetVisibility(true, true);
			Prim->SetHiddenInGame(false, true);
		}
	}
}

void AMobaGroundMarker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
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

	if (ViewTarget && (ViewTarget == GetOwner() || ViewTarget == GetInstigator()))
	{
		return false;
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner() ? GetOwner() : GetInstigator());
	if (OwnerPawn && RealViewer == OwnerPawn->GetController())
	{
		return false;
	}

	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}
