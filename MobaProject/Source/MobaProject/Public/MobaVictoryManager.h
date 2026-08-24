#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MobaVictoryManager.generated.h"

class AMobaTower;
class UMobaEndHUD;

UCLASS(Blueprintable)
class MOBAPROJECT_API AMobaVictoryManager : public AActor
{
	GENERATED_BODY()

public:
	AMobaVictoryManager();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void NotifyTowerDestroyed(AMobaTower* Tower);
	void NotifyLoading(bool bPlayAgain);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastLoading(bool bPlayAgain);

protected:
	void CheckTowers();
	void EndMatch(int32 DestroyedTowerTeam);
	void ShowLocalEndScreen();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastMatchOver(int32 InWinningTeam);

	UFUNCTION()
	void OnRep_MatchOver();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	TObjectPtr<AMobaTower> Team1Tower;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	TObjectPtr<AMobaTower> Team2Tower;

	UPROPERTY(ReplicatedUsing = OnRep_MatchOver)
	bool bMatchOver = false;

	UPROPERTY(Replicated)
	int32 WinningTeam = 0;

	UPROPERTY()
	TObjectPtr<UMobaEndHUD> EndHUD;
};
