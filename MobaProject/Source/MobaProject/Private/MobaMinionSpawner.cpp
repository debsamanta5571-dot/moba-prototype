#include "MobaMinionSpawner.h"
#include "MobaMinion.h"
#include "MobaTower.h"
#include "Kismet/GameplayStatics.h"

AMobaMinionSpawner::AMobaMinionSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

void AMobaMinionSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		SpawnWave();
		GetWorldTimerManager().SetTimer(WaveTimer, this, &AMobaMinionSpawner::SpawnWave, WaveInterval, true);
	}
}

void AMobaMinionSpawner::SpawnWave()
{
	if (!MinionClass)
	{
		return;
	}

	const FVector Base = GetActorLocation();
	const FVector Right = GetActorRightVector();

	for (int32 i = 0; i < MinionsPerWave; ++i)
	{
		const FVector Loc = Base + Right * (i - (MinionsPerWave - 1) * 0.5f) * 80.f;
		FTransform SpawnTM(GetActorRotation(), Loc);
		if (AMobaMinion* Minion = GetWorld()->SpawnActorDeferred<AMobaMinion>(
			MinionClass,
			SpawnTM,
			this,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
		{
			Minion->SetGoalTower(GoalTower);
			UGameplayStatics::FinishSpawningActor(Minion, SpawnTM);
			Minion->SetTeamId(TeamID);
		}
	}
}
