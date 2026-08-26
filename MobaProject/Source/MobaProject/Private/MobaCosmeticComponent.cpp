#include "MobaCosmeticComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "MobaBaseCharacter.h"

UMobaCosmeticComponent::UMobaCosmeticComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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

void UMobaCosmeticComponent::TryAttachHat()
{
	AMobaBaseCharacter* Hero = GetHero();
	UStaticMeshComponent* Hat = Hero ? Hero->GetHat() : nullptr;
	if (!Hero || !Hat)
	{
		return;
	}

	UStaticMesh* Stolen = Hat->GetStaticMesh();
	TArray<UStaticMeshComponent*> ExtraHats;
	Hero->GetComponents<UStaticMeshComponent>(ExtraHats);
	for (UStaticMeshComponent* Extra : ExtraHats)
	{
		if (!Extra || Extra == Hat)
		{
			continue;
		}
		if (!Stolen && Extra->GetStaticMesh())
		{
			Stolen = Extra->GetStaticMesh();
		}
		Extra->DestroyComponent();
	}

	if (!Stolen)
	{
		const FString ClassName = Hero->GetClass()->GetName();
		const TCHAR* Path = ClassName.Contains(TEXT("Mage"))
			? TEXT("/Game/Moba/Art/WizardHat.WizardHat")
			: TEXT("/Game/Moba/Art/FlatCap.FlatCap");
		Stolen = LoadObject<UStaticMesh>(nullptr, Path);
	}
	if (Stolen && Hat->GetStaticMesh() != Stolen)
	{
		Hat->SetStaticMesh(Stolen);
	}

	Hat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
