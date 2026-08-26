#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MobaMenuGameMode.generated.h"

UCLASS()
class MOBAPROJECT_API AMobaMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMobaMenuGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

protected:
	virtual void BeginPlay() override;

	void AssignLobbyTeam(AController* Player);
	void EnsureLobbyLeader();

	int32 NextJoinTeam = 1;
};
