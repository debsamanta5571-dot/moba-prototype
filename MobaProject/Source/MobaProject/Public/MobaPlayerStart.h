#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "MobaPlayerStart.generated.h"

UCLASS()
class MOBAPROJECT_API AMobaPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	AMobaPlayerStart(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	int32 TeamID = 1;
};
