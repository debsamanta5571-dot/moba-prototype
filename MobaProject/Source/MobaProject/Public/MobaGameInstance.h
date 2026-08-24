#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "MobaGameInstance.generated.h"

class APlayerState;
class UNetDriver;
struct FWorldContext;

UCLASS(config = Game)
class MOBAPROJECT_API UMobaGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	UFUNCTION(BlueprintCallable, Category = "Moba")
	void HostGame();

	UFUNCTION(BlueprintCallable, Category = "Moba")
	void JoinGame(const FString& Address);

	UFUNCTION(BlueprintCallable, Category = "Moba")
	void RestartMatch();

	UFUNCTION(BlueprintCallable, Category = "Moba")
	void ReturnToMenu();

	void ShowMenu();
	void ShowLobby();
	bool ShouldShowLobby() const;
	void ShowLoadingScreen(const FString& Message = TEXT("LOADING..."));
	void HideLoadingScreen();
	bool IsShowingLoading() const;
	UFUNCTION()
	void NotifyLocalMapReady();
	void OnMatchUnlocked();
	int32 CountSessionPlayers() const;
	void StartMatchFromLobby();
	void LeaveLobby();
	void RequestLobbyTeam(int32 Team);
	int32 GetPendingTeamId() const { return PendingTeamId; }
	void SetSelectedHeroIndex(int32 Index);
	int32 GetSelectedHeroIndex() const { return SelectedHeroIndex; }
	int32 GetHeroChoiceCount() const;
	FString GetHeroDisplayName(int32 Index) const;
	TSubclassOf<class AMobaBaseCharacter> GetHeroClassAt(int32 Index) const;
	void ApplyLocalHeroChoice();
	int32 TakeCachedLobbyHero(const APlayerState* PS);
	void SetSimulatedPingRange(int32 MinMs, int32 MaxMs);
	int32 GetSimulatedPingMin() const { return SimulatedPingMinMs; }
	int32 GetSimulatedPingMax() const { return SimulatedPingMaxMs; }
	void ApplySimulatedPing();
	int32 TakeCachedLobbyTeam(const APlayerState* PS);
	void ShowSettings();
	void HideSettings();
	void ToggleSettings();

	float GetMasterVolume() const { return MasterVolume; }
	void SetMasterVolume(float Volume);
	void SetGraphicsQuality(int32 Level);
	int32 GetGraphicsQuality() const;

	virtual void LoadComplete(const float LoadTime, const FString& MapName) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Moba")
	FName ArenaMap = TEXT("/Game/Moba/Maps/MobaTestMap");

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Moba")
	FName MenuMap = TEXT("/Game/Moba/Maps/MobaMenu");

protected:
	void ReleaseMenuInput();
	void HideMenu();
	void HideLobby();
	void BroadcastLoading(bool bPlayAgain);
	void SendClientsToMenu();
	bool IsMenuMap(const FString& MapName) const;
	bool IsInLobbyNet() const;
	void ApplySettingsInput();
	void ApplyLobbyInput();
	void CacheLobbyTeams();
	void HandlePreLoadMap(const FWorldContext& LoadedContext, const FString& MapName);
	void SetupMovieLoadingScreen(const FString& Message);
	void StopLoadingMovie();
	void ApplySavedGraphics();
	static FString LobbyPlayerKey(const APlayerState* PS);

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

	bool TickJoinPoll(float DeltaTime);

	void EnterLobbyFromJoin();
	void ClearJoinTimers();
	void EnsurePingTimer();
	bool TryFinishJoin() const;
	void FailJoin(const FString& Message);
	void AbortJoinConnection();
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

	UPROPERTY(config)
	float MasterVolume = 0.5f;

	UPROPERTY()
	TObjectPtr<class UMobaMenuWidget> MenuWidget;

	UPROPERTY()
	TObjectPtr<class UMobaLobbyWidget> LobbyWidget;

	UPROPERTY()
	TObjectPtr<class UMobaSettingsWidget> SettingsWidget;

	UPROPERTY()
	TObjectPtr<class UMobaLoadingWidget> LoadingWidget;

	int32 PendingTeamId = 0;
	int32 SelectedHeroIndex = 0;
	int32 SimulatedPingMinMs = 0;
	int32 SimulatedPingMaxMs = 0;
	bool bLoadingScreenQueued = false;
	bool bAttemptingJoin = false;
	bool bJoinAborted = false;
	bool bLobbySession = false;
	double IgnoreNetFailUntil = 0.0;
	FString PendingJoinUrl;
	FString JoinErrorMessage;
	TMap<FString, int32> LobbyTeamByPlayer;
	TMap<FString, int32> LobbyHeroByPlayer;
};
