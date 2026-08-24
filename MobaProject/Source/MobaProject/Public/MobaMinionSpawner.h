#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MobaMinionSpawner.generated.h"

class AMobaMinion;
class AMobaTower;

UCLASS(Blueprintable)
class MOBAPROJECT_API AMobaMinionSpawner : public AActor
{
	GENERATED_BODY()

public:
	AMobaMinionSpawner();

protected:
	virtual void BeginPlay() override;

	void SpawnWave();

	UFUNCTION()
	void TryStartWaves();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	TSubclassOf<AMobaMinion> MinionClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	int32 TeamID = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	TObjectPtr<AMobaTower> GoalTower;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	int32 MinionsPerWave = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float WaveInterval = 20.f;

	FTimerHandle WaveTimer;
	FTimerHandle WaitMatchTimer;
};
