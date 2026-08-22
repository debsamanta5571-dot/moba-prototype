#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AMobaGameMode.generated.h"

UCLASS()
class MOBAPROJECT_API AAMobaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAMobaGameMode();

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot) override;

protected:
	void AssignTeam(APlayerController* Player);
	int32 CountTeam(int32 TeamId) const;
	int32 GetStartTeamId(const AActor* Start) const;
};
