#include "MobaTower.h"
#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "EngineUtils.h"
#include "MobaAttributeSet.h"
#include "MobaBaseCharacter.h"
#include "MobaCombatLibrary.h"
#include "MobaHealthWidget.h"
#include "MobaMinion.h"
#include "MobaProjectile.h"
#include "MobaSfx.h"
#include "MobaVictoryManager.h"
#include "Net/UnrealNetwork.h"

AMobaTower::AMobaTower()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetCanBeDamaged(true);
	SetActorEnableCollision(true);

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->SetBoxExtent(FVector(150.f, 150.f, 200.f));
	Collision->SetRelativeLocation(FVector::ZeroVector);
	Collision->CanCharacterStepUpOn = ECB_No;
	Collision->SetMobility(EComponentMobility::Movable);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetupAttachment(Collision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetMobility(EComponentMobility::Movable);

	FirePoint = CreateDefaultSubobject<USceneComponent>(TEXT("FirePoint"));
	FirePoint->SetupAttachment(Collision);
	FirePoint->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	FirePoint->SetMobility(EComponentMobility::Movable);

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UMobaAttributeSet>(TEXT("AttributeSet"));

	HealthWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthWidget"));
	HealthWidget->SetupAttachment(Collision);
	HealthWidget->SetUsingAbsoluteScale(true);
	HealthWidget->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	HealthWidget->SetWidgetSpace(EWidgetSpace::Screen);
	HealthWidget->SetPivot(FVector2D(0.5f, 1.f));
	HealthWidget->SetDrawSize(FVector2D(160.f, 40.f));
	HealthWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthWidget->SetWidgetClass(UMobaHealthWidget::StaticClass());

	ConfigureCollision();
}

void AMobaTower::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMobaTower, TeamID);
	DOREPLIFETIME(AMobaTower, bDead);
}

UAbilitySystemComponent* AMobaTower::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

float AMobaTower::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.f;
}

const UPrimitiveComponent* AMobaTower::GetBlockingCollision() const
{
	return Collision.Get();
}

void AMobaTower::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ConfigureCollision();
}

void AMobaTower::ConfigureCollision()
{
	SetActorEnableCollision(true);

	if (Collision)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Collision->SetCollisionObjectType(ECC_WorldDynamic);
		Collision->SetCollisionResponseToAllChannels(ECR_Block);
		Collision->SetGenerateOverlapEvents(true);
		Collision->CanCharacterStepUpOn = ECB_No;
		Collision->SetHiddenInGame(true);
	}

	FBox MeshBounds(EForceInit::ForceInit);
	bool bHasMesh = false;

	TArray<UPrimitiveComponent*> Prims;
	GetComponents<UPrimitiveComponent>(Prims);
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (!Prim || Prim == Collision)
		{
			continue;
		}
		if (Cast<UWidgetComponent>(Prim))
		{
			Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			continue;
		}
		if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Prim))
		{
			MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (MeshComp->GetStaticMesh() && Collision)
			{
				const FTransform MeshToCollision = MeshComp->GetComponentTransform().GetRelativeTransform(
					Collision->GetComponentTransform());
				MeshBounds += MeshComp->CalcBounds(MeshToCollision).GetBox();
				bHasMesh = true;
			}
			continue;
		}
		if (Cast<UShapeComponent>(Prim))
		{
			Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	if (Collision && bHasMesh)
	{
		Collision->SetBoxExtent(MeshBounds.GetExtent().GetAbs().ComponentMax(FVector(40.f, 40.f, 40.f)));
	}

	PlaceHealthWidget();
}

void AMobaTower::PlaceHealthWidget()
{
	if (!HealthWidget)
	{
		return;
	}

	HealthWidget->SetUsingAbsoluteScale(true);
	HealthWidget->SetWorldScale3D(FVector::OneVector);
	HealthWidget->SetWidgetSpace(EWidgetSpace::Screen);
	HealthWidget->SetPivot(FVector2D(0.5f, 1.f));
	HealthWidget->SetDrawSize(FVector2D(160.f, 40.f));
	HealthWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	FVector Loc = GetActorLocation();
	if (Collision)
	{
		Collision->UpdateBounds();
		Loc = Collision->Bounds.Origin;
		Loc.Z += Collision->Bounds.BoxExtent.Z + 40.f;
	}
	HealthWidget->SetWorldLocation(Loc);
}

void AMobaTower::BeginPlay()
{
	Super::BeginPlay();

	if (Collision)
	{
		USceneComponent* OldRoot = GetRootComponent();
		if (OldRoot && OldRoot != Collision)
		{
			Collision->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			SetRootComponent(Collision);
			OldRoot->AttachToComponent(Collision, FAttachmentTransformRules::KeepWorldTransform);
		}
	}
	ConfigureCollision();
	if (AttributeSet)
	{
		AttributeSet->InitMaxHealth(MaxHealth);
		AttributeSet->InitHealth(MaxHealth);
		AttributeSet->InitDamageModifier(DamageModifier);
		AttributeSet->InitDamageResistance(DamageResistance);
		AttributeSet->InitMoveSpeed(MoveSpeed);
		AttributeSet->InitGoldOnKill(GoldOnKill);
	}
	if (AbilitySystemComponent)
	{
		if (AttributeSet)
		{
			AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());
		}
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

	AActor* Target = ChooseFireTarget();
	if (!Target || !ProjectileClass || !GetWorld())
	{
		return;
	}

	const FVector Start = FirePoint ? FirePoint->GetComponentLocation() : GetActorLocation();
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
		const float Dist = FVector::Dist(Start, Target->GetActorLocation());
		const float Life = FMath::Max(ProjectileLifetime, Dist / FMath::Max(ProjectileSpeed, 1.f) + 2.5f);
		Bolt->InitHoming(Target, ProjectileSpeed, Damage, Life);
	}

	MulticastFireSfx();
}

void AMobaTower::MulticastFireSfx_Implementation()
{
	const FVector Loc = FirePoint ? FirePoint->GetComponentLocation() : GetActorLocation();
	UMobaSfx::Play(this, FireSound, EMobaSfx::TowerFire, Loc);
}

bool AMobaTower::IsValidEnemyInRange(const AActor* Other) const
{
	if (!IsValid(Other) || Other == this || bDead)
	{
		return false;
	}
	if (!MobaIsEnemy(this, Other))
	{
		return false;
	}
	float Health = 0.f;
	if (const AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Other))
	{
		Health = Hero->GetHealth();
	}
	else if (const AMobaMinion* Minion = Cast<AMobaMinion>(Other))
	{
		Health = Minion->GetHealth();
	}
	else
	{
		return false;
	}
	if (Health <= 0.f)
	{
		return false;
	}
	const float R = FMath::Max(Range, 0.f);
	return FVector::DistSquared(GetActorLocation(), Other->GetActorLocation()) <= R * R;
}

AActor* AMobaTower::FindClosestMinion() const
{
	AActor* Best = nullptr;
	float BestDistSq = Range * Range;
	const FVector Here = GetActorLocation();
	for (TActorIterator<AMobaMinion> It(GetWorld()); It; ++It)
	{
		AMobaMinion* Minion = *It;
		if (!IsValidEnemyInRange(Minion))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Here, Minion->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Minion;
		}
	}
	return Best;
}

AActor* AMobaTower::FindClosestHero() const
{
	AActor* Best = nullptr;
	float BestDistSq = Range * Range;
	const FVector Here = GetActorLocation();
	for (TActorIterator<AMobaBaseCharacter> It(GetWorld()); It; ++It)
	{
		AMobaBaseCharacter* Hero = *It;
		if (!IsValidEnemyInRange(Hero))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Here, Hero->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Hero;
		}
	}
	return Best;
}

AActor* AMobaTower::ChooseFireTarget()
{
	AActor* Current = CurrentTarget.Get();
	if (IsValidEnemyInRange(Current) && Cast<AMobaBaseCharacter>(Current))
	{
		return Current;
	}
	if (AActor* Minion = FindClosestMinion())
	{
		CurrentTarget = Minion;
		return Minion;
	}
	if (IsValidEnemyInRange(Current))
	{
		return Current;
	}
	CurrentTarget = FindClosestHero();
	return CurrentTarget.Get();
}

void AMobaTower::PullAggro(AActor* Attacker)
{
	if (!HasAuthority() || !IsValidEnemyInRange(Attacker))
	{
		return;
	}
	AActor* Current = CurrentTarget.Get();
	if (IsValidEnemyInRange(Current) && Cast<AMobaBaseCharacter>(Current))
	{
		return;
	}
	CurrentTarget = Attacker;
}

void AMobaTower::NotifyHeroDamagedHero(AMobaBaseCharacter* Attacker, AMobaBaseCharacter* Victim)
{
	if (!IsValid(Attacker) || !IsValid(Victim) || Attacker == Victim)
	{
		return;
	}
	UWorld* World = Attacker->GetWorld();
	if (!World || !World->GetAuthGameMode())
	{
		return;
	}
	for (TActorIterator<AMobaTower> It(World); It; ++It)
	{
		AMobaTower* Tower = *It;
		if (!Tower || Tower->bDead)
		{
			continue;
		}
		if (!MobaIsEnemy(Tower, Attacker) || MobaIsEnemy(Tower, Victim))
		{
			continue;
		}
		Tower->PullAggro(Attacker);
	}
}

void AMobaTower::OnRep_Dead()
{
	if (!bDead)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(FireTimer);
	SetActorEnableCollision(false);
	if (HealthWidget)
	{
		HealthWidget->SetHiddenInGame(true);
	}
}

void AMobaTower::HandleDeath()
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	CurrentTarget = nullptr;
	OnRep_Dead();
	if (HasAuthority() && GetWorld())
	{
		for (TActorIterator<AMobaVictoryManager> It(GetWorld()); It; ++It)
		{
			It->NotifyTowerDestroyed(this);
		}
	}
}
