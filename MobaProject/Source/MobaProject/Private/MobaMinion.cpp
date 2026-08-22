#include "MobaMinion.h"
#include "AbilitySystemComponent.h"
#include "AMobaPlayerState.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
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
	if (const AMobaPlayerState* PS = Cast<AMobaPlayerState>(Actor))
	{
		return PS->TeamID;
	}
	if (const AMobaMinion* Minion = Cast<AMobaMinion>(Actor))
	{
		return Minion->GetTeamId();
	}
	if (const AMobaTower* Tower = Cast<AMobaTower>(Actor))
	{
		return Tower->GetTeamId();
	}
	if (const AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Actor))
	{
		if (Hero->GetTeamId() != 0)
		{
			return Hero->GetTeamId();
		}
		if (const AMobaPlayerState* HeroPS = Hero->GetPlayerState<AMobaPlayerState>())
		{
			return HeroPS->TeamID;
		}
		return 0;
	}
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		if (const AMobaPlayerState* PawnPS = Pawn->GetPlayerState<AMobaPlayerState>())
		{
			return PawnPS->TeamID;
		}
	}
	return 0;
}

bool MobaIsEnemy(const AActor* A, const AActor* B)
{
	const int32 TeamA = MobaTeamIdOf(A);
	const int32 TeamB = MobaTeamIdOf(B);
	return TeamA != 0 && TeamB != 0 && TeamA != TeamB;
}

FLinearColor MobaAttitudeColor(const AActor* Viewer, const AActor* Target)
{
	if (MobaIsEnemy(Viewer, Target))
	{
		return FLinearColor(1.f, 0.42f, 0.42f, 1.f);
	}
	return FLinearColor(0.45f, 0.72f, 1.f, 1.f);
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
	HealthWidget->SetPivot(FVector2D(0.5f, 1.f));
	HealthWidget->SetDrawSize(FVector2D(120.f, 36.f));
	HealthWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthWidget->SetWidgetClass(UMobaHealthWidget::StaticClass());
}

void AMobaMinion::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMobaMinion, TeamID);
	DOREPLIFETIME(AMobaMinion, bDead);
	DOREPLIFETIME(AMobaMinion, bStunned);
	DOREPLIFETIME(AMobaMinion, SlowMul);
	DOREPLIFETIME(AMobaMinion, StunUntilTime);
	DOREPLIFETIME(AMobaMinion, SlowUntilTime);
	DOREPLIFETIME(AMobaMinion, HasteUntilTime);
}

UAbilitySystemComponent* AMobaMinion::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

float AMobaMinion::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.f;
}

float AMobaMinion::GetGoldOnKill() const
{
	return AttributeSet ? AttributeSet->GetGoldOnKill() : 0.f;
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
		AttributeSet->InitGoldOnKill(GoldOnKill);
		AttributeSet->InitMoveSpeed(MoveSpeed);
		AttributeSet->InitDamageModifier(DamageModifier);
		AttributeSet->InitDamageResistance(DamageResistance);
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
	if (!GoalTower)
	{
		GoalTower = FindEnemyTower();
	}
}

void AMobaMinion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SanitizeTimedState();
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

	if (bStunned)
	{
		return;
	}

	if (GetWorld()->GetTimeSeconds() - LastAttackTime < AttackInterval)
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->bOrientRotationToMovement = false;
		}
		if (AActor* Target = CombatTarget.Get())
		{
			FaceActor(Target);
		}
		return;
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
	}

	AActor* Target = CombatTarget.Get();
	const bool bKeepUnit = Target && !Cast<AMobaTower>(Target) && ShouldKeepTarget(Target);
	if (!bKeepUnit)
	{
		Target = AcquireNewTarget();
		CombatTarget = Target;
	}
	if (!Target)
	{
		return;
	}
	Engage(Target);
}

void AMobaMinion::Engage(AActor* Target)
{
	if (DistanceToAttackTarget(Target) <= AttackRange)
	{
		TryAttack(Target);
		return;
	}
	Chase(Target);
}

void AMobaMinion::Chase(AActor* Target)
{
	MoveToward(GetAttackApproachPoint(Target));
}

void AMobaMinion::NotifyDamagedBy(AActor* DamageCauser)
{
	AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(DamageCauser);
	if (!IsValidCombatTarget(Hero))
	{
		return;
	}
	CombatTarget = Hero;
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
	CombatTarget = Target;
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

	if (DistanceToAttackTarget(Target) > AttackRange * 1.5f)
	{
		return;
	}

	AMobaBaseCharacter::ApplyMobaDamage(Target, AttackDamage, this);
}

void AMobaMinion::ApplyStatus(const FMobaEffectSpec& Spec)
{
	if (bDead || !HasAuthority())
	{
		return;
	}

	switch (Spec.Type)
	{
	case EMobaEffectType::Heal:
		if (AttributeSet && Spec.Magnitude > 0.f)
		{
			AttributeSet->SetHealth(FMath::Min(AttributeSet->GetMaxHealth(), AttributeSet->GetHealth() + Spec.Magnitude));
		}
		break;

	case EMobaEffectType::Stun:
		if (Spec.Duration > 0.f)
		{
			bStunned = true;
			StunUntilTime = GetServerTimeSeconds() + Spec.Duration;
			GetWorldTimerManager().SetTimer(StunTimer, this, &AMobaMinion::ClearStun, Spec.Duration, false);
			RefreshMoveSpeed();
		}
		break;

	case EMobaEffectType::Slow:
		if (Spec.Duration > 0.f)
		{
			SlowMul = FMath::Clamp(Spec.Magnitude, 0.f, 1.f);
			SlowUntilTime = GetServerTimeSeconds() + Spec.Duration;
			GetWorldTimerManager().SetTimer(SlowTimer, this, &AMobaMinion::ClearSlow, Spec.Duration, false);
			RefreshMoveSpeed();
		}
		break;

	case EMobaEffectType::MoveSpeed:
		if (Spec.Duration > 0.f)
		{
			HasteMul = FMath::Max(Spec.Magnitude, 0.f);
			HasteUntilTime = GetServerTimeSeconds() + Spec.Duration;
			GetWorldTimerManager().SetTimer(HasteTimer, this, &AMobaMinion::ClearHaste, Spec.Duration, false);
			RefreshMoveSpeed();
		}
		break;

	default:
		break;
	}
}

void AMobaMinion::RefreshMoveSpeed()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement || bDead)
	{
		return;
	}

	if (bStunned)
	{
		Movement->DisableMovement();
		Movement->StopMovementImmediately();
		return;
	}

	if (Movement->MovementMode == MOVE_None)
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
	const float BaseSpeed = (AttributeSet && AttributeSet->GetMoveSpeed() > 0.f)
		? AttributeSet->GetMoveSpeed()
		: MoveSpeed;
	Movement->MaxWalkSpeed = BaseSpeed * SlowMul * HasteMul;
}

void AMobaMinion::ClearStun()
{
	bStunned = false;
	StunUntilTime = 0.f;
	GetWorldTimerManager().ClearTimer(StunTimer);
	RefreshMoveSpeed();
}

void AMobaMinion::ClearSlow()
{
	SlowMul = 1.f;
	SlowUntilTime = 0.f;
	GetWorldTimerManager().ClearTimer(SlowTimer);
	RefreshMoveSpeed();
}

void AMobaMinion::ClearHaste()
{
	HasteMul = 1.f;
	HasteUntilTime = 0.f;
	GetWorldTimerManager().ClearTimer(HasteTimer);
	RefreshMoveSpeed();
}

float AMobaMinion::GetServerTimeSeconds() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}
		return World->GetTimeSeconds();
	}
	return 0.f;
}

bool AMobaMinion::IsTimerExpired(float UntilTime) const
{
	return UntilTime <= KINDA_SMALL_NUMBER || GetServerTimeSeconds() >= UntilTime;
}

void AMobaMinion::SanitizeTimedState()
{
	if (bStunned && IsTimerExpired(StunUntilTime))
	{
		ClearStun();
	}
	if (SlowMul < 0.99f && IsTimerExpired(SlowUntilTime))
	{
		ClearSlow();
	}
	if (HasteMul > 1.01f && IsTimerExpired(HasteUntilTime))
	{
		ClearHaste();
	}
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
		if (!MobaIsEnemy(this, Hero))
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

AActor* AMobaMinion::FindClosestMinion() const
{
	AActor* Best = nullptr;
	float BestDistSq = AggroRange * AggroRange;
	const FVector Here = GetActorLocation();

	for (TActorIterator<AMobaMinion> It(GetWorld()); It; ++It)
	{
		AMobaMinion* Other = *It;
		if (!Other || Other == this || Other->GetHealth() <= 0.f)
		{
			continue;
		}
		if (!MobaIsEnemy(this, Other))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Here, Other->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Other;
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

float AMobaMinion::DistanceToAttackTarget(const AActor* Target) const
{
	if (!Target)
	{
		return TNumericLimits<float>::Max();
	}

	if (const AMobaTower* Tower = Cast<AMobaTower>(Target))
	{
		if (const UPrimitiveComponent* Block = Tower->GetBlockingCollision())
		{
			FVector Closest;
			const float Dist = Block->GetClosestPointOnCollision(GetActorLocation(), Closest);
			if (Dist >= 0.f)
			{
				return Dist;
			}
		}
		const FBox Bounds = Target->GetComponentsBoundingBox();
		return FMath::Sqrt(Bounds.ComputeSquaredDistanceToPoint(GetActorLocation()));
	}

	return FVector::Dist(GetActorLocation(), Target->GetActorLocation());
}

FVector AMobaMinion::GetAttackApproachPoint(const AActor* Target) const
{
	if (!Target)
	{
		return GetActorLocation();
	}

	if (const AMobaTower* Tower = Cast<AMobaTower>(Target))
	{
		if (const UPrimitiveComponent* Block = Tower->GetBlockingCollision())
		{
			FVector Closest;
			if (Block->GetClosestPointOnCollision(GetActorLocation(), Closest) >= 0.f)
			{
				return Closest;
			}
		}
	}

	return Target->GetActorLocation();
}

bool AMobaMinion::IsValidCombatTarget(const AActor* Target) const
{
	if (!IsValid(Target) || !MobaIsEnemy(this, Target))
	{
		return false;
	}
	if (const AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Target))
	{
		return Hero->GetHealth() > 0.f && !Hero->IsDead();
	}
	if (const AMobaMinion* Other = Cast<AMobaMinion>(Target))
	{
		return Other->GetHealth() > 0.f;
	}
	if (const AMobaTower* Tower = Cast<AMobaTower>(Target))
	{
		return Tower->GetHealth() > 0.f;
	}
	return false;
}

bool AMobaMinion::ShouldKeepTarget(const AActor* Target) const
{
	if (!IsValidCombatTarget(Target))
	{
		return false;
	}
	if (Cast<AMobaTower>(Target))
	{
		return false;
	}
	return DistanceToAttackTarget(Target) <= AggroRange * 2.f;
}

AActor* AMobaMinion::AcquireNewTarget()
{
	if (AActor* Hero = FindClosestHero())
	{
		return Hero;
	}
	if (AActor* Other = FindClosestMinion())
	{
		return Other;
	}
	if (!GoalTower || GoalTower->GetHealth() <= 0.f || GoalTower->GetTeamId() == TeamID)
	{
		GoalTower = FindEnemyTower();
	}
	return GoalTower;
}

void AMobaMinion::PlayDeathAnimation()
{
	USkeletalMeshComponent* Skel = GetMesh();
	if (!Skel || !DeathAnimation)
	{
		return;
	}

	StopAnimMontage();
	Skel->PlayAnimation(DeathAnimation, false);
}

void AMobaMinion::ApplyDeathPresentation()
{
	PlayDeathAnimation();
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->DisableMovement();
		Movement->StopMovementImmediately();
	}
	SetActorEnableCollision(false);
	if (HealthWidget)
	{
		HealthWidget->SetHiddenInGame(true);
	}
}

void AMobaMinion::OnRep_Dead()
{
	if (bDead)
	{
		ApplyDeathPresentation();
	}
}

void AMobaMinion::HandleDeath()
{
	if (bDead || !HasAuthority())
	{
		return;
	}
	bDead = true;
	GetWorldTimerManager().ClearTimer(AttackHitTimer);
	GetWorldTimerManager().ClearTimer(StunTimer);
	GetWorldTimerManager().ClearTimer(SlowTimer);
	GetWorldTimerManager().ClearTimer(HasteTimer);
	bStunned = false;
	SlowMul = 1.f;
	HasteMul = 1.f;
	StunUntilTime = 0.f;
	SlowUntilTime = 0.f;
	HasteUntilTime = 0.f;
	PendingAttackTarget.Reset();
	CombatTarget.Reset();
	ApplyDeathPresentation();

	float CorpseTime = 2.f;
	if (const UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(DeathAnimation))
	{
		CorpseTime = FMath::Max(CorpseTime, Seq->GetPlayLength() + 0.4f);
	}
	SetLifeSpan(CorpseTime);
}
