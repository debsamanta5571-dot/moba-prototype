#include "MobaCosmeticComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "MobaBaseCharacter.h"

UMobaCosmeticComponent::UMobaCosmeticComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(false);
}

AMobaBaseCharacter* UMobaCosmeticComponent::GetHero() const
{
	return Cast<AMobaBaseCharacter>(GetOwner());
}

void UMobaCosmeticComponent::BeginPlay()
{
	Super::BeginPlay();
	TryAttachHat();
}

void UMobaCosmeticComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bHatAttached)
	{
		TryAttachHat();
	}
	if (bHatAttached)
	{
		SetComponentTickEnabled(false);
	}
}

void UMobaCosmeticComponent::TryAttachHat()
{
	AMobaBaseCharacter* Hero = GetHero();
	if (bHatAttached || !Hero || Hero->GetLocalRole() != ROLE_SimulatedProxy)
	{
		if (Hero && Hero->GetLocalRole() != ROLE_SimulatedProxy)
		{
			bHatAttached = true;
			SetComponentTickEnabled(false);
		}
		return;
	}

	USkeletalMeshComponent* Body = Hero->GetMesh();
	if (!Body || !Body->DoesSocketExist(TEXT("Head")))
	{
		return;
	}

	// Blueprint CS uses KeepWorld -> Head. On a SimulatedProxy, bone transforms are
	// still identity at construct/BeginPlay, so Head is at the mesh origin (feet).
	// KeepWorld then stores "stay ~173uu above Head". When the skeleton updates,
	// Head jumps to the skull and the hat rides 173uu above it.
	Body->RefreshBoneTransforms();
	const FTransform HeadInMesh = Body->GetSocketTransform(TEXT("Head"), RTS_Component);
	if (HeadInMesh.GetLocation().Z < 80.f)
	{
		return;
	}

	TArray<UStaticMeshComponent*> Hats;
	Hero->GetComponents<UStaticMeshComponent>(Hats);
	if (Hats.Num() == 0)
	{
		bHatAttached = true;
		SetComponentTickEnabled(false);
		return;
	}

	const FAttachmentTransformRules Rules(
		EAttachmentRule::SnapToTarget,
		EAttachmentRule::SnapToTarget,
		EAttachmentRule::KeepWorld,
		false);

	for (UStaticMeshComponent* Hat : Hats)
	{
		if (!IsValid(Hat) || !Hat->GetStaticMesh())
		{
			continue;
		}

		// Mesh-space rest pose (not Head-relative). Converted below once Head.Z is actually at the skull.
		FVector Loc(-1.278287f, 11.350370f, 173.364543f);
		FRotator Rot(-2.012294f, -3.550484f, 9.074668f);
		FVector Scale(0.096447f);
		if (Hat->GetStaticMesh()->GetPathName().Contains(TEXT("WizardHat")))
		{
			Loc = FVector(-3.627649f, 10.705580f, 174.569992f);
			Rot = FRotator(0.f, 0.f, 0.f);
			Scale = FVector(0.205844f);
		}

		const FTransform HatInMesh(Rot, Loc, FVector(1.f));
		const FTransform HatInHead = HatInMesh.GetRelativeTransform(HeadInMesh);

		Hat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Hat->AttachToComponent(Body, Rules, TEXT("Head"));
		Hat->SetRelativeLocationAndRotation(HatInHead.GetLocation(), HatInHead.Rotator());
		Hat->SetRelativeScale3D(Scale);
	}

	bHatAttached = true;
	SetComponentTickEnabled(false);
}
