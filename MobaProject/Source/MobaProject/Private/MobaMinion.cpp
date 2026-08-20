#include "MobaMinion.h"
#include "AbilitySystemComponent.h"
#include "AMobaPlayerState.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "MobaAttributeSet.h"
#include "MobaBaseCharacter.h"
#include "MobaHealthWidget.h"
#include "MobaTower.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

int32 MobaTeamIdOf(const AActor* Actor)
{
	if (!Actor)
	{
		return 0;
	}
	if (const AMobaMinion* Minion = Cast<AMobaMinion>(Actor))
	{
		return Minion->GetTeamId();
	}
	if (const AMobaTower* Tower = Cast<AMobaTower>(Actor))
	{
		return Tower->GetTeamId();
	}
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		if (const AMobaPlayerState* PS = Pawn->GetPlayerState<AMobaPlayerState>())
		{
			return PS->TeamID;
		}
	}
	return 0;
}

AMobaMinion::AMobaMinion()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(24.f, 48.f);

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 480.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 320.f;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UMobaAttributeSet>(TEXT("AttributeSet"));

	HealthWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthWidget"));
	HealthWidget->SetupAttachment(GetCapsuleComponent());
	HealthWidget->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	HealthWidget->SetWidgetSpace(EWidgetSpace::Screen);
	HealthWidget->SetDrawSize(FVector2D(90.f, 12.f));
	HealthWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthWidget->SetWidgetClass(UMobaHealthWidget::StaticClass());
}

void AMobaMinion::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMobaMinion, TeamID);
}

UAbilitySystemComponent* AMobaMinion::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

float AMobaMinion::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.f;
}

void AMobaMinion::SetTeamId(int32 InTeam)
{
	TeamID = InTeam;
}

void AMobaMinion::SetGoalTower(AMobaTower* Tower)
{
	GoalTower = Tower;
}

void AMobaMinion::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
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
	if (!GoalTower)
	{
		GoalTower = FindEnemyTower();
	}
}

void AMobaMinion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && !bDead)
	{
		Think();
	}
}

void AMobaMinion::Think()
{
	if (GetHealth() <= 0.f)
	{
		HandleDeath();
		return;
	}

	if (GetWorld()->GetTimeSeconds() - LastAttackTime < AttackInterval)
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->bOrientRotationToMovement = false;
		}
		if (AActor* Target = PendingAttackTarget.Get())
		{
			FaceActor(Target);
		}
		return;
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
	}

	if (AActor* Hero = FindClosestHero())
	{
		if (FVector::Dist(GetActorLocation(), Hero->GetActorLocation()) <= AttackRange)
		{
			TryAttack(Hero);
			return;
		}
		Chase(Hero);
		return;
	}

	if (!GoalTower || GoalTower->GetHealth() <= 0.f)
	{
		GoalTower = FindEnemyTower();
	}
	if (!GoalTower)
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), GoalTower->GetActorLocation()) <= AttackRange)
	{
		TryAttack(GoalTower);
		return;
	}
	Chase(GoalTower);
}

void AMobaMinion::Chase(AActor* Target)
{
	MoveToward(Target->GetActorLocation());
}

void AMobaMinion::TryAttack(AActor* Target)
{
	if (!Target)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastAttackTime < AttackInterval)
	{
		return;
	}
	LastAttackTime = Now;
	PendingAttackTarget = Target;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = false;
	}
	FaceActor(Target);
	MulticastPlayAttack();

	const float HitDelay = FMath::Clamp(AttackHitDelay, 0.f, AttackInterval);
	if (HitDelay <= 0.f)
	{
		DealAttackDamage();
	}
	else
	{
		GetWorldTimerManager().SetTimer(AttackHitTimer, this, &AMobaMinion::DealAttackDamage, HitDelay, false);
	}
}

void AMobaMinion::FaceActor(AActor* Target)
{
	if (!Target)
	{
		return;
	}

	FVector To = Target->GetActorLocation() - GetActorLocation();
	To.Z = 0.f;
	if (!To.IsNearlyZero())
	{
		SetActorRotation(To.Rotation());
	}
}

void AMobaMinion::DealAttackDamage()
{
	if (bDead)
	{
		return;
	}

	AActor* Target = PendingAttackTarget.Get();
	if (!Target)
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), Target->GetActorLocation()) > AttackRange * 1.5f)
	{
		return;
	}

	AMobaBaseCharacter::ApplyMobaDamage(Target, AttackDamage, this);
}

void AMobaMinion::MulticastPlayAttack_Implementation()
{
	if (AttackMontage)
	{
		PlayAnimMontage(AttackMontage);
	}
}

AActor* AMobaMinion::FindClosestHero() const
{
	AActor* Best = nullptr;
	float BestDistSq = AggroRange * AggroRange;
	const FVector Here = GetActorLocation();

	for (TActorIterator<AMobaBaseCharacter> It(GetWorld()); It; ++It)
	{
		AMobaBaseCharacter* Hero = *It;
		if (!Hero || Hero->GetHealth() <= 0.f || Hero->IsDead())
		{
			continue;
		}
		const int32 OtherTeam = MobaTeamIdOf(Hero);
		if (OtherTeam == 0 || OtherTeam == TeamID)
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

AMobaTower* AMobaMinion::FindEnemyTower() const
{
	AMobaTower* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	const FVector Here = GetActorLocation();

	for (TActorIterator<AMobaTower> It(GetWorld()); It; ++It)
	{
		AMobaTower* Tower = *It;
		if (!Tower || Tower->GetHealth() <= 0.f)
		{
			continue;
		}
		if (Tower->GetTeamId() == 0 || Tower->GetTeamId() == TeamID)
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Here, Tower->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Tower;
		}
	}

	return Best;
}

void AMobaMinion::MoveToward(const FVector& Dest)
{
	FVector Dir = Dest - GetActorLocation();
	Dir.Z = 0.f;
	if (Dir.SizeSquared() < 400.f)
	{
		return;
	}
	AddMovementInput(Dir.GetSafeNormal(), 1.f);
}

void AMobaMinion::HandleDeath()
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	GetWorldTimerManager().ClearTimer(AttackHitTimer);
	PendingAttackTarget.Reset();
	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);
	if (HealthWidget)
	{
		HealthWidget->SetHiddenInGame(true);
	}
	SetLifeSpan(1.5f);
}
