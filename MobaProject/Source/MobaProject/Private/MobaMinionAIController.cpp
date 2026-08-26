#include "MobaMinionAIController.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MobaBaseCharacter.h"
#include "MobaCombatLibrary.h"
#include "MobaMinion.h"
#include "MobaTower.h"

AMobaMinionAIController::AMobaMinionAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	bAttachToPawn = true;
}

void AMobaMinionAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ClearCombat();
}

void AMobaMinionAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AMobaMinion* Minion = GetMinion();
	if (Minion && Minion->HasAuthority() && !Minion->IsDead())
	{
		Think();
	}
}

AMobaMinion* AMobaMinionAIController::GetMinion() const
{
	return Cast<AMobaMinion>(GetPawn());
}

void AMobaMinionAIController::NotifyDamagedBy(AActor* DamageCauser)
{
	AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(DamageCauser);
	if (!IsValidCombatTarget(Hero))
	{
		return;
	}
	SetCombatTarget(Hero);
}

void AMobaMinionAIController::NotifyAttackHit(AActor* Target)
{
	if (ChasedPlayer.Get() == Target)
	{
		bDamagedChasedPlayer = true;
	}
}

void AMobaMinionAIController::ClearCombat()
{
	CombatTarget.Reset();
	ChasedPlayer.Reset();
	LeashIgnoredPlayer.Reset();
	ChasePlayerStartTime = 0.f;
	bDamagedChasedPlayer = false;
}

void AMobaMinionAIController::Think()
{
	AMobaMinion* Minion = GetMinion();
	UWorld* World = GetWorld();
	if (!Minion || !World)
	{
		return;
	}

	if (Minion->GetHealth() <= 0.f)
	{
		Minion->HandleDeath();
		return;
	}

	if (Minion->IsStunned())
	{
		return;
	}

	if (World->GetTimeSeconds() - Minion->GetLastAttackTime() < Minion->GetAttackInterval())
	{
		if (UCharacterMovementComponent* Move = Minion->GetCharacterMovement())
		{
			Move->bOrientRotationToMovement = false;
		}
		if (AActor* Target = CombatTarget.Get())
		{
			Minion->FaceActor(Target);
		}
		return;
	}

	if (UCharacterMovementComponent* Move = Minion->GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
	}

	if (AMobaBaseCharacter* Ignored = LeashIgnoredPlayer.Get())
	{
		const float Aggro = Minion->GetAggroRange();
		if (!IsValid(Ignored)
			|| Ignored->GetHealth() <= 0.f
			|| Ignored->IsDead()
			|| FVector::DistSquared(Minion->GetActorLocation(), Ignored->GetActorLocation()) > Aggro * Aggro)
		{
			LeashIgnoredPlayer.Reset();
		}
	}

	if (HasFailedPlayerChase())
	{
		GiveUpPlayerChase();
	}

	AActor* Target = CombatTarget.Get();
	const bool bKeepUnit = Target && !Cast<AMobaTower>(Target) && ShouldKeepTarget(Target);
	if (!bKeepUnit)
	{
		Target = AcquireNewTarget();
		SetCombatTarget(Target);
	}
	if (!Target)
	{
		return;
	}
	Engage(Target);
}

void AMobaMinionAIController::Engage(AActor* Target)
{
	AMobaMinion* Minion = GetMinion();
	if (!Minion)
	{
		return;
	}
	if (DistanceToAttackTarget(Target) <= Minion->GetAttackRange())
	{
		Minion->TryAttack(Target);
		return;
	}
	Chase(Target);
}

void AMobaMinionAIController::Chase(AActor* Target)
{
	MoveToward(GetAttackApproachPoint(Target));
}

AActor* AMobaMinionAIController::FindClosestHero() const
{
	AMobaMinion* Minion = GetMinion();
	if (!Minion)
	{
		return nullptr;
	}

	AActor* Best = nullptr;
	const float Aggro = Minion->GetAggroRange();
	float BestDistSq = Aggro * Aggro;
	const FVector Here = Minion->GetActorLocation();

	for (TActorIterator<AMobaBaseCharacter> It(GetWorld()); It; ++It)
	{
		AMobaBaseCharacter* Hero = *It;
		if (!Hero || Hero->GetHealth() <= 0.f || Hero->IsDead())
		{
			continue;
		}
		if (!MobaIsEnemy(Minion, Hero))
		{
			continue;
		}
		if (LeashIgnoredPlayer.Get() == Hero)
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

int32 AMobaMinionAIController::CountAlliedMinionsTargeting(const AActor* Target) const
{
	AMobaMinion* Minion = GetMinion();
	if (!Minion || !Target)
	{
		return 0;
	}

	int32 Count = 0;
	for (TActorIterator<AMobaMinion> It(GetWorld()); It; ++It)
	{
		const AMobaMinion* Ally = *It;
		if (!Ally || Ally == Minion || Ally->GetHealth() <= 0.f)
		{
			continue;
		}
		if (Ally->GetTeamId() != Minion->GetTeamId())
		{
			continue;
		}
		if (Ally->GetCombatTarget() == Target)
		{
			++Count;
		}
	}
	return Count;
}

AActor* AMobaMinionAIController::FindClosestMinion() const
{
	AMobaMinion* Minion = GetMinion();
	if (!Minion)
	{
		return nullptr;
	}

	AActor* Best = nullptr;
	int32 BestFocus = TNumericLimits<int32>::Max();
	float BestDistSq = TNumericLimits<float>::Max();
	const float RangeSq = FMath::Square(Minion->GetAggroRange());
	const FVector Here = Minion->GetActorLocation();

	for (TActorIterator<AMobaMinion> It(GetWorld()); It; ++It)
	{
		AMobaMinion* Other = *It;
		if (!Other || Other == Minion || Other->GetHealth() <= 0.f)
		{
			continue;
		}
		if (!MobaIsEnemy(Minion, Other))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Here, Other->GetActorLocation());
		if (DistSq > RangeSq)
		{
			continue;
		}

		const int32 Focus = CountAlliedMinionsTargeting(Other);
		if (Focus < BestFocus || (Focus == BestFocus && DistSq < BestDistSq))
		{
			BestFocus = Focus;
			BestDistSq = DistSq;
			Best = Other;
		}
	}
	return Best;
}

void AMobaMinionAIController::MoveToward(const FVector& Dest)
{
	AMobaMinion* Minion = GetMinion();
	if (!Minion)
	{
		return;
	}
	FVector Dir = Dest - Minion->GetActorLocation();
	Dir.Z = 0.f;
	if (Dir.SizeSquared() < 400.f)
	{
		return;
	}
	Minion->AddMovementInput(Dir.GetSafeNormal(), 1.f);
}

float AMobaMinionAIController::DistanceToAttackTarget(const AActor* Target) const
{
	AMobaMinion* Minion = GetMinion();
	if (!Minion || !Target)
	{
		return TNumericLimits<float>::Max();
	}

	if (const AMobaTower* Tower = Cast<AMobaTower>(Target))
	{
		if (const UPrimitiveComponent* Block = Tower->GetBlockingCollision())
		{
			FVector Closest;
			const float Dist = Block->GetClosestPointOnCollision(Minion->GetActorLocation(), Closest);
			if (Dist >= 0.f)
			{
				return Dist;
			}
		}
		const FBox Bounds = Target->GetComponentsBoundingBox();
		return FMath::Sqrt(Bounds.ComputeSquaredDistanceToPoint(Minion->GetActorLocation()));
	}

	return FVector::Dist(Minion->GetActorLocation(), Target->GetActorLocation());
}

FVector AMobaMinionAIController::GetAttackApproachPoint(const AActor* Target) const
{
	AMobaMinion* Minion = GetMinion();
	if (!Minion || !Target)
	{
		return Minion ? Minion->GetActorLocation() : FVector::ZeroVector;
	}

	if (const AMobaTower* Tower = Cast<AMobaTower>(Target))
	{
		if (const UPrimitiveComponent* Block = Tower->GetBlockingCollision())
		{
			FVector Closest;
			if (Block->GetClosestPointOnCollision(Minion->GetActorLocation(), Closest) >= 0.f)
			{
				return Closest;
			}
		}
	}
	return Target->GetActorLocation();
}

bool AMobaMinionAIController::IsValidCombatTarget(const AActor* Target) const
{
	AMobaMinion* Minion = GetMinion();
	if (!Minion || !IsValid(Target) || !MobaIsEnemy(Minion, Target))
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

bool AMobaMinionAIController::ShouldKeepTarget(const AActor* Target) const
{
	AMobaMinion* Minion = GetMinion();
	if (!Minion || !IsValidCombatTarget(Target))
	{
		return false;
	}
	if (Cast<AMobaTower>(Target))
	{
		return false;
	}
	if (HasFailedPlayerChase() && Target == ChasedPlayer.Get())
	{
		return false;
	}
	return DistanceToAttackTarget(Target) <= Minion->GetAggroRange() * 2.f;
}

AActor* AMobaMinionAIController::AcquireNewTarget()
{
	AMobaMinion* Minion = GetMinion();
	if (!Minion)
	{
		return nullptr;
	}
	if (AActor* Hero = FindClosestHero())
	{
		return Hero;
	}
	if (AActor* Other = FindClosestMinion())
	{
		return Other;
	}
	AMobaTower* Goal = Minion->GetGoalTower();
	if (!Goal || Goal->GetHealth() <= 0.f || Goal->GetTeamId() == Minion->GetTeamId())
	{
		Goal = Minion->FindEnemyTower();
		Minion->SetGoalTower(Goal);
	}
	return Goal;
}

void AMobaMinionAIController::SetCombatTarget(AActor* Target)
{
	CombatTarget = Target;
	RefreshPlayerChase(Target);
}

void AMobaMinionAIController::RefreshPlayerChase(AActor* Target)
{
	AMobaMinion* Minion = GetMinion();
	AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Target);
	if (!Hero)
	{
		ChasedPlayer.Reset();
		ChasePlayerStartTime = 0.f;
		bDamagedChasedPlayer = false;
		return;
	}
	if (ChasedPlayer.Get() != Hero)
	{
		ChasedPlayer = Hero;
		ChasePlayerStartTime = Minion ? Minion->GetServerTimeSeconds() : 0.f;
		bDamagedChasedPlayer = false;
	}
}

bool AMobaMinionAIController::HasFailedPlayerChase() const
{
	AMobaMinion* Minion = GetMinion();
	AMobaBaseCharacter* Hero = ChasedPlayer.Get();
	if (!Minion || !Hero || bDamagedChasedPlayer || Minion->GetPlayerChaseGiveUpSeconds() <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	if (CombatTarget.Get() != Hero)
	{
		return false;
	}
	return Minion->GetServerTimeSeconds() - ChasePlayerStartTime >= Minion->GetPlayerChaseGiveUpSeconds();
}

void AMobaMinionAIController::GiveUpPlayerChase()
{
	if (AMobaBaseCharacter* Hero = ChasedPlayer.Get())
	{
		LeashIgnoredPlayer = Hero;
	}
	ChasedPlayer.Reset();
	ChasePlayerStartTime = 0.f;
	bDamagedChasedPlayer = false;
	CombatTarget.Reset();
}
