#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MobaMenuGameMode.generated.h"

class UMobaMenuWidget;

UCLASS()
class MOBAPROJECT_API AMobaMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMobaMenuGameMode();

protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	TObjectPtr<UMobaMenuWidget> MenuWidget;
};
