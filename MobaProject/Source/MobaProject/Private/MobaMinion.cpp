#include "MobaMinion.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/DamageType.h"
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
#include "MobaCombatLibrary.h"
#include "MobaHealthWidget.h"
#include "MobaStatusComponent.h"
#include "MobaTower.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "MobaMinionAIController.h"

AMobaMinion::AMobaMinion()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(24.f, 48.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		Skel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Skel->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bEnablePhysicsInteraction = false;
	Move->MaxDepenetrationWithPawn = 200.f;
	Move->MaxDepenetrationWithPawnAsProxy = 100.f;
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 480.f, 0.f);
	Move->MaxWalkSpeed = 320.f;
	Move->bUseRVOAvoidance = true;
	Move->AvoidanceConsiderationRadius = 180.f;
	SetNetUpdateFrequency(30.f);
	SetMinNetUpdateFrequency(15.f);

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UMobaAttributeSet>(TEXT("AttributeSet"));
	Status = CreateDefaultSubobject<UMobaStatusComponent>(TEXT("Status"));

	HealthWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthWidget"));
	HealthWidget->SetupAttachment(GetCapsuleComponent());
	HealthWidget->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	HealthWidget->SetWidgetSpace(EWidgetSpace::Screen);
	HealthWidget->SetPivot(FVector2D(0.5f, 1.f));
	HealthWidget->SetDrawSize(FVector2D(120.f, 36.f));
	HealthWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthWidget->SetWidgetClass(UMobaHealthWidget::StaticClass());

	AIControllerClass = AMobaMinionAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AMobaMinion::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMobaMinion, TeamID);
	DOREPLIFETIME(AMobaMinion, bDead);
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

	if (UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		Cap->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Cap->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Cap->SetGenerateOverlapEvents(true);
	}
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		Skel->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	if (AttributeSet)
	{
		AttributeSet->InitMaxHealth(MaxHealth);
		AttributeSet->InitHealth(MaxHealth);
		AttributeSet->InitGoldOnKill(GoldOnKill);
		AttributeSet->InitMoveSpeed(MoveSpeed);
		AttributeSet->InitDamageModifier(DamageModifier);
		AttributeSet->InitDamageResistance(DamageResistance);
		AttributeSet->InitCooldownReduction(0.f);
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
	RefreshMoveSpeed();
}

void AMobaMinion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SanitizeTimedState();
}

AActor* AMobaMinion::GetCombatTarget() const
{
	if (const AMobaMinionAIController* AI = Cast<AMobaMinionAIController>(GetController()))
	{
		return AI->GetCombatTarget();
	}
	return nullptr;
}

void AMobaMinion::NotifyDamagedBy(AActor* DamageCauser)
{
	if (AMobaMinionAIController* AI = Cast<AMobaMinionAIController>(GetController()))
	{
		AI->NotifyDamagedBy(DamageCauser);
	}
}

void AMobaMinion::NotePlayerDamageFrom(AMobaBaseCharacter* Player)
{
	if (!HasAuthority() || !IsValid(Player))
	{
		return;
	}

	const float Window = FMath::Max(0.f, PlayerKillCreditSeconds);
	if (Window <= KINDA_SMALL_NUMBER)
	{
		ClearPlayerKillCredit();
		return;
	}

	LastPlayerDamager = Player;
	PlayerKillCreditUntilTime = GetServerTimeSeconds() + Window;
	GetWorldTimerManager().ClearTimer(PlayerKillCreditTimer);
	GetWorldTimerManager().SetTimer(
		PlayerKillCreditTimer,
		this,
		&AMobaMinion::ClearPlayerKillCredit,
		Window,
		false);
}

AMobaBaseCharacter* AMobaMinion::GetPlayerKillCredit() const
{
	if (IsTimerExpired(PlayerKillCreditUntilTime))
	{
		return nullptr;
	}
	AMobaBaseCharacter* Player = LastPlayerDamager.Get();
	return IsValid(Player) ? Player : nullptr;
}

void AMobaMinion::ClearPlayerKillCredit()
{
	LastPlayerDamager.Reset();
	PlayerKillCreditUntilTime = 0.f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlayerKillCreditTimer);
	}
}

void AMobaMinion::TryAttack(AActor* Target)
{
	if (!Target)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	if (Now - LastAttackTime < AttackInterval)
	{
		return;
	}
	LastAttackTime = Now;
	if (AMobaMinionAIController* AI = Cast<AMobaMinionAIController>(GetController()))
	{
		AI->SetCombatTarget(Target);
	}
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
	AActor* Target = PendingAttackTarget.Get();
	PendingAttackTarget.Reset();
	if (!IsValid(Target) || !HasAuthority())
	{
		return;
	}

	if (DistanceToAttackTarget(Target) > AttackRange * 1.5f)
	{
		return;
	}

	if (UMobaCombatLibrary::ApplyMobaDamage(Target, AttackDamage, this))
	{
		if (AMobaMinionAIController* AI = Cast<AMobaMinionAIController>(GetController()))
		{
			AI->NotifyAttackHit(Target);
		}
	}
}

bool AMobaMinion::IsStunned() const
{
	return Status && Status->IsStunned();
}

bool AMobaMinion::IsSlowed() const
{
	return Status && Status->IsSlowed();
}

void AMobaMinion::ApplyStatus(const FMobaEffectSpec& Spec)
{
	if (Status)
	{
		Status->ApplySpec(Spec);
	}
}

void AMobaMinion::RefreshMoveSpeed()
{
	if (Status)
	{
		Status->RefreshMoveSpeed();
	}
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
	if (Status)
	{
		Status->SanitizeTimedState();
	}
	if (LastPlayerDamager.IsValid() && IsTimerExpired(PlayerKillCreditUntilTime))
	{
		ClearPlayerKillCredit();
	}
}

void AMobaMinion::MulticastPlayAttack_Implementation()
{
	if (AttackMontage)
	{
		PlayAnimMontage(AttackMontage);
	}
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

void AMobaMinion::HandleDeath(AActor* Killer)
{
	if (bDead || !HasAuthority())
	{
		return;
	}
	UMobaCombatLibrary::AwardKillGold(this, Killer);
	bDead = true;
	ClearPlayerKillCredit();
	if (Status)
	{
		Status->ClearAll();
	}
	if (AMobaMinionAIController* AI = Cast<AMobaMinionAIController>(GetController()))
	{
		AI->ClearCombat();
	}
	ApplyDeathPresentation();

	float CorpseTime = 2.f;
	if (const UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(DeathAnimation))
	{
		CorpseTime = FMath::Max(CorpseTime, Seq->GetPlayLength() + 0.4f);
	}
	SetLifeSpan(CorpseTime);
}

void AMobaMinion::FellOutOfWorld(const UDamageType& DmgType)
{
	(void)DmgType;
	if (bDead)
	{
		return;
	}
	if (AttributeSet)
	{
		AttributeSet->SetHealth(0.f);
	}
	HandleDeath();
}
