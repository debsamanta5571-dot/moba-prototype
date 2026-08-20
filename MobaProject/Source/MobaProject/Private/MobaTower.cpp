#include "MobaTower.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "MobaHealthWidget.h"
#include "EngineUtils.h"
#include "MobaAttributeSet.h"
#include "MobaBaseCharacter.h"
#include "MobaMinion.h"
#include "MobaProjectile.h"
#include "Net/UnrealNetwork.h"

AMobaTower::AMobaTower()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetupAttachment(SceneRoot);
	Collision->SetBoxExtent(FVector(80.f, 80.f, 160.f));
	Collision->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	Collision->SetCollisionObjectType(ECC_Pawn);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UMobaAttributeSet>(TEXT("AttributeSet"));

	HealthWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthWidget"));
	HealthWidget->SetupAttachment(SceneRoot);
	HealthWidget->SetRelativeLocation(FVector(0.f, 0.f, 280.f));
	HealthWidget->SetWidgetSpace(EWidgetSpace::Screen);
	HealthWidget->SetDrawSize(FVector2D(160.f, 16.f));
	HealthWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthWidget->SetWidgetClass(UMobaHealthWidget::StaticClass());
}

void AMobaTower::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMobaTower, TeamID);
}

UAbilitySystemComponent* AMobaTower::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

float AMobaTower::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.f;
}

void AMobaTower::BeginPlay()
{
	Super::BeginPlay();

	if (AttributeSet)
	{
		AttributeSet->InitMaxHealth(MaxHealth);
		AttributeSet->InitHealth(MaxHealth);
	}
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	if (HealthWidget)
	{
		HealthWidget->InitWidget();
		if (UMobaHealthWidget* UI = Cast<UMobaHealthWidget>(HealthWidget->GetWidget()))
		{
			UI->SetOwnerActor(this);
		}
	}

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(FireTimer, this, &AMobaTower::Fire, FireInterval, true);
	}
}

void AMobaTower::Fire()
{
	if (bDead || GetHealth() <= 0.f)
	{
		HandleDeath();
		return;
	}

	AActor* Target = FindClosestEnemy();
	if (!Target || !ProjectileClass)
	{
		return;
	}

	const FVector Start = GetActorLocation() + FireOffset;
	FVector Dir = Target->GetActorLocation() - Start;
	if (Dir.IsNearlyZero())
	{
		return;
	}
	Dir.Normalize();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;

	if (AMobaProjectile* Bolt = GetWorld()->SpawnActor<AMobaProjectile>(
		ProjectileClass,
		Start,
		Dir.Rotation(),
		Params))
	{
		Bolt->InitFlight(Dir, ProjectileSpeed, Damage, ProjectileLifetime, false);
	}
}

AActor* AMobaTower::FindClosestEnemy() const
{
	AActor* Best = nullptr;
	float BestDistSq = Range * Range;
	const FVector Here = GetActorLocation();

	auto Consider = [&](AActor* Other)
	{
		if (!IsValid(Other) || Other == this)
		{
			return;
		}
		const int32 OtherTeam = MobaTeamIdOf(Other);
		if (OtherTeam == 0 || OtherTeam == TeamID)
		{
			return;
		}
		const float DistSq = FVector::DistSquared(Here, Other->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Other;
		}
	};

	for (TActorIterator<AMobaBaseCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetHealth() > 0.f)
		{
			Consider(*It);
		}
	}
	for (TActorIterator<AMobaMinion> It(GetWorld()); It; ++It)
	{
		if (It->GetHealth() > 0.f)
		{
			Consider(*It);
		}
	}

	return Best;
}

void AMobaTower::HandleDeath()
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	GetWorldTimerManager().ClearTimer(FireTimer);
	SetActorEnableCollision(false);
	if (HealthWidget)
	{
		HealthWidget->SetHiddenInGame(true);
	}
}
