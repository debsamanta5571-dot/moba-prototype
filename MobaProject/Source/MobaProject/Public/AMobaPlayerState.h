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

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Moba")
	int32 HeroIndex = 0;

	UFUNCTION(Server, Reliable)
	void ServerSetTeam(int32 NewTeam);

	UFUNCTION(Server, Reliable)
	void ServerSetHeroIndex(int32 NewIndex);

	UFUNCTION(Client, Reliable)
	void ClientShowLoading(const FString& Message);

	UFUNCTION(Server, Reliable)
	void ServerNotifyMapLoaded();

	void MarkMapLoaded();
	bool HasLoadedMap() const { return bMapLoaded; }
	bool IsMatchUnlocked() const { return bMatchUnlocked; }

	void AssignLobbyName();

	UFUNCTION()
	void OnRep_TeamId();

	UFUNCTION()
	void OnRep_MatchUnlocked();

	UPROPERTY(ReplicatedUsing = OnRep_MatchUnlocked)
	bool bMatchUnlocked = false;

	bool bMapLoaded = false;
};
