#include "MobaShop.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "MobaBaseCharacter.h"
#include "Net/UnrealNetwork.h"

AMobaShop::AMobaShop()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true; // team id has to reach people who aren't standing in the fountain

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
	Capsule->SetCanEverAffectNavigation(false);
}

void AMobaShop::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMobaShop, TeamID);
}

bool AMobaShop::ContainsPawn(const APawn* Pawn) const
{
	if (!Pawn || !Capsule)
	{
		return false;
	}

	// Overlap events miss if the pawn spawned already inside. We do a capsule vs capsule ourselves.
	FVector HeroCenter = Pawn->GetActorLocation();
	float HeroRadius = 42.f;
	float HeroHalfHeight = 96.f;
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (const UCapsuleComponent* HeroCap = Character->GetCapsuleComponent())
		{
			HeroCenter = HeroCap->GetComponentLocation();
			HeroRadius = HeroCap->GetScaledCapsuleRadius();
			HeroHalfHeight = HeroCap->GetScaledCapsuleHalfHeight();
		}
	}

	const FVector ShopCenter = Capsule->GetComponentLocation();
	const float ShopRadius = Capsule->GetScaledCapsuleRadius();
	const float ShopHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Combined = ShopRadius + HeroRadius;
	const float DistSq2D = (ShopCenter - HeroCenter).SizeSquared2D();
	if (DistSq2D > Combined * Combined)
	{
		return false;
	}

	const float ShopMin = ShopCenter.Z - ShopHalfHeight;
	const float ShopMax = ShopCenter.Z + ShopHalfHeight;
	const float HeroMin = HeroCenter.Z - HeroHalfHeight;
	const float HeroMax = HeroCenter.Z + HeroHalfHeight;
	const float ZGap = FMath::Max(0.f, FMath::Max(ShopMin - HeroMax, HeroMin - ShopMax));
	return DistSq2D + ZGap * ZGap <= Combined * Combined;
}

void AMobaShop::BeginPlay()
{
	Super::BeginPlay();

	if (!Capsule)
	{
		return;
	}

	Capsule->SetCollisionObjectType(ECC_WorldDynamic);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Capsule->SetGenerateOverlapEvents(true);
	Capsule->OnComponentBeginOverlap.AddDynamic(this, &AMobaShop::OnBeginOverlap);
	Capsule->OnComponentEndOverlap.AddDynamic(this, &AMobaShop::OnEndOverlap);
	Capsule->UpdateOverlaps();

	for (TActorIterator<AMobaBaseCharacter> It(GetWorld()); It; ++It)
	{
		if (AMobaBaseCharacter* Hero = *It)
		{
			Hero->ScheduleShopRangeRefresh();
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
		Hero->RefreshShopRange();
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
		Hero->RefreshShopRange();
	}
}
