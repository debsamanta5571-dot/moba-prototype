#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MobaGameMode.generated.h"

UCLASS()
class MOBAPROJECT_API AMobaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMobaGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual void StartPlay() override;

	void NotifyPlayerLoaded();
	bool IsMatchUnlocked() const { return bMatchUnlocked; }
	void SpawnLateJoiner(AController* Player);

protected:
	void DestroyOrphanHeroes();
	void AssignTeam(AController* Player);
	int32 NextTeamId();
	int32 GetStartTeamId(const AActor* Start) const;
	void TryUnlockMatch(bool bForce);
	void UnlockMatch();

	UFUNCTION()
	void OnWaitForPlayersTimeout();

	int32 NextJoinTeam = 1;
	int32 ExpectedPlayers = 1;
	bool bMatchUnlocked = false;
	FTimerHandle WaitPlayersTimer;
};
