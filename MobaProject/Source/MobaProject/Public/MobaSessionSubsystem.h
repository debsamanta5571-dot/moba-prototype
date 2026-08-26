#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MobaSessionSubsystem.generated.h"

class APlayerState;
class UNetDriver;
class UMobaFrontEndSubsystem;
class UMobaGameInstance;
class AMobaBaseCharacter;

// Host, join, travel, lobby cache, simulated ping. Widgets stay on FrontEnd.
UCLASS()
class MOBAPROJECT_API UMobaSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void HostGame();
	void JoinGame(const FString& Address);
	void RestartMatch();
	void ReturnToMenu();
	void StartMatchFromLobby();
	void AuthorityStartMatchFromLobby();
	bool IsLocalLobbyLeader() const;
	void LeaveLobby();
	void RequestLobbyTeam(int32 Team);
	int32 GetPendingTeamId() const { return PendingTeamId; }
	void SetSelectedHeroIndex(int32 Index);
	int32 GetSelectedHeroIndex() const { return SelectedHeroIndex; }
	int32 GetHeroChoiceCount() const;
	FString GetHeroDisplayName(int32 Index) const;
	TSubclassOf<AMobaBaseCharacter> GetHeroClassAt(int32 Index) const;
	void ApplyLocalHeroChoice();
	int32 TakeCachedLobbyHero(const APlayerState* PS);
	int32 TakeCachedLobbyTeam(const APlayerState* PS);
	void SetSimulatedPingRange(int32 MinMs, int32 MaxMs);
	int32 GetSimulatedPingMin() const { return SimulatedPingMinMs; }
	int32 GetSimulatedPingMax() const { return SimulatedPingMaxMs; }
	void ApplySimulatedPing();
	bool ShouldShowLobby() const;
	bool ShouldShowJoinLoadout() const;
	bool IsJoinLoadout() const { return bJoinLoadout; }
	void ConfirmJoinLoadout();
	void NotifyLocalMapReady();
	void OnMatchUnlocked();
	int32 CountSessionPlayers() const;
	void HandleLoadComplete(const FString& MapName);
	bool ConsumeJoinError(FString& OutMessage);
	void SetJoinLoadout(bool bJoin) { bJoinLoadout = bJoin; }
	FName GetArenaMap() const;
	FName GetMenuMap() const;

protected:
	UMobaGameInstance* GetMobaGI() const;
	UMobaFrontEndSubsystem* GetFrontEnd() const;
	bool IsMenuMap(const FString& MapName) const;
	bool IsInLobbyNet() const;
	void CacheLobbyTeams();
	static void CollectLobbyKeys(const APlayerState* PS, TArray<FString>& OutKeys);
	void BroadcastLoading(bool bPlayAgain);
	void SendClientsToMenu();
	void ClearJoinTimers();
	void EnsurePingTimer();
	bool TryFinishJoin() const;
	void FailJoin(const FString& Message);
	void AbortJoinConnection();
	void EnterLobbyFromJoin();

	UFUNCTION()
	void DoRestartTravel();

	UFUNCTION()
	void DoMenuTravel();

	UFUNCTION()
	void OnJoinTimeout();

	UFUNCTION()
	void PollJoin();

	UFUNCTION()
	void BeginJoinTravel();

	UFUNCTION()
	void NotifyLocalMapReadyRetry();

	bool TickJoinPoll(float DeltaTime);
	void OnEngineNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void OnEngineTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	FTimerHandle TravelTimer;
	FTimerHandle MapReadyTimer;
	FTimerHandle JoinTimeoutTimer;
	FTimerHandle JoinPollTimer;
	FTimerHandle JoinTravelTimer;
	FTimerHandle PingApplyTimer;
	FTSTicker::FDelegateHandle JoinPollTicker;
	double JoinGiveUpTime = 0.0;
	int32 PendingTeamId = 0;
	int32 SelectedHeroIndex = 0;
	int32 SimulatedPingMinMs = 0;
	int32 SimulatedPingMaxMs = 0;
	bool bAttemptingJoin = false;
	bool bJoinAborted = false;
	bool bLobbySession = false;
	bool bJoinLoadout = false;
	double IgnoreNetFailUntil = 0.0;
	FString PendingJoinUrl;
	FString JoinErrorMessage;
	TMap<FString, int32> LobbyTeamByPlayer;
	TMap<FString, int32> LobbyHeroByPlayer;
};
