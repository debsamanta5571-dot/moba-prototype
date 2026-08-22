#include "MobaShop.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "MobaBaseCharacter.h"

AMobaShop::AMobaShop()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->SetupAttachment(SceneRoot);
	Capsule->InitCapsuleSize(500.f, 280.f);
	Capsule->SetRelativeLocation(FVector(0.f, 0.f, 280.f));
	Capsule->SetCollisionObjectType(ECC_WorldDynamic);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Capsule->SetGenerateOverlapEvents(true);
	Capsule->SetHiddenInGame(false);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(SceneRoot);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 220.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(48.f);
	Label->SetTextRenderColor(FColor(242, 209, 71));
	Label->SetText(FText::FromString(TEXT("SHOP")));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 20.f));
	Mesh->SetRelativeScale3D(FVector(10.f, 10.f, 0.4f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
}

void AMobaShop::BeginPlay()
{
	Super::BeginPlay();

	if (Mesh && !Mesh->GetStaticMesh())
	{
		if (UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
		{
			Mesh->SetStaticMesh(Cylinder);
		}
	}

	if (!Capsule)
	{
		return;
	}

	Capsule->OnComponentBeginOverlap.AddDynamic(this, &AMobaShop::OnBeginOverlap);
	Capsule->OnComponentEndOverlap.AddDynamic(this, &AMobaShop::OnEndOverlap);

	TArray<AActor*> Overlapping;
	Capsule->GetOverlappingActors(Overlapping, AMobaBaseCharacter::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Actor))
		{
			Hero->NotifyEnteredShop();
		}
	}
}

void AMobaShop::OnBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(OtherActor))
	{
		Hero->NotifyEnteredShop();
	}
}

void AMobaShop::OnEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(OtherActor))
	{
		Hero->NotifyLeftShop();
	}
}
