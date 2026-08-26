#include "MobaGroundMarker.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GA_MobaGroundAoE.h"
#include "GameFramework/Pawn.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
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
	if (!Mesh)
	{
		return;
	}

	UStaticMesh* Used = DisplayMesh;
	if (!Used)
	{
		Used = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	}
	if (Used && Mesh->GetStaticMesh() != Used)
	{
		Mesh->SetStaticMesh(Used);
	}
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetHiddenInGame(false);
	Mesh->SetVisibility(true, true);

	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Moba/Art/M_MobaBolt.M_MobaBolt"));
	if (!Base)
	{
		Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	if (Base)
	{
		BlastMid = Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, Base);
		if (BlastMid)
		{
			BlastMid->SetVectorParameterValue(TEXT("BoltColor"), FLinearColor(1.f, 0.28f, 0.04f, 1.f));
			BlastMid->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.28f, 0.04f, 1.f));
		}
	}
}

void AMobaGroundMarker::SetRadiusScale(float Radius, float HeightScale)
{
	const float Scale = FMath::Max(Radius, 1.f) / 50.f;
	const FVector NewScale(Scale, Scale, FMath::Max(HeightScale, 0.12f));
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
	bCosmetic = bInCosmetic;
	ElapsedTime = 0.f;
	TargetRadius = FMath::Max(Radius, 80.f);
	BlastDuration = FMath::Max(Lifetime, 0.2f);
	SetReplicates(false);
	ApplyDisplayMesh();
	SetRadiusScale(TargetRadius * 0.08f, 0.04f);
	ShowAllVisuals();
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
	if (!bExpanding)
	{
		return;
	}
	ElapsedTime += DeltaSeconds;
	const float Total = FMath::Max(BlastDuration, 0.2f);
	const float T = FMath::Clamp(ElapsedTime / Total, 0.f, 1.f);
	const float Grow = FMath::InterpEaseOut(0.08f, 1.f, T, 2.f);
	SetRadiusScale(TargetRadius * Grow, 0.04f);
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
