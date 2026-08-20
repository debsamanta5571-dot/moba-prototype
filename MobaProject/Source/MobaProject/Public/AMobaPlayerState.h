#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AMobaPlayerState.generated.h"

UCLASS()
class MOBAPROJECT_API AMobaPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AMobaPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_TeamId, BlueprintReadOnly, Category = "Moba")
	int32 TeamID = 0;

	UFUNCTION()
	void OnRep_TeamId();
};
