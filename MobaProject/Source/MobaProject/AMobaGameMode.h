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

	virtual void PostLogin(APlayerController* NewPlayer) override;

	int32 NextTeamId = 0;
};
