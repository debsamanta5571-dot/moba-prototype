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

	UFUNCTION(Server, Reliable)
	void ServerConfirmLoadout(int32 NewHero, int32 NewTeam);

	UFUNCTION(Client, Reliable)
	void ClientShowLoading(const FString& Message);

	UFUNCTION(Server, Reliable)
	void ServerNotifyMapLoaded();

	void MarkMapLoaded();
	bool HasLoadedMap() const { return bMapLoaded; }
	bool IsMatchUnlocked() const { return bMatchUnlocked; }
	bool IsAwaitingLoadout() const { return bAwaitingLoadout; }
	void SetAwaitingLoadout(bool bAwaiting);
	bool CanEditLoadout() const;

	void AssignLobbyName();
	bool IsLobbyLeader() const { return bLobbyLeader; }

	UFUNCTION(Server, Reliable)
	void ServerStartMatchFromLobby();

	UFUNCTION()
	void OnRep_TeamId();

	UFUNCTION()
	void OnRep_MatchUnlocked();

	UFUNCTION()
	void OnRep_AwaitingLoadout();

	UPROPERTY(ReplicatedUsing = OnRep_MatchUnlocked)
	bool bMatchUnlocked = false;

	UPROPERTY(ReplicatedUsing = OnRep_AwaitingLoadout)
	bool bAwaitingLoadout = false;

	UPROPERTY(Replicated)
	bool bLobbyLeader = false;

	bool bMapLoaded = false;
};
