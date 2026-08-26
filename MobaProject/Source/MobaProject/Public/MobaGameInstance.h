#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MobaGameInstance.generated.h"

class APlayerState;
class UMobaFrontEndSubsystem;
class UMobaSessionSubsystem;

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
	void RestoreUiPointerIfNeeded();
	bool IsMenuUiActive() const;
	void ShowLobby();
	bool ShouldShowLobby() const;
	bool ShouldShowJoinLoadout() const;
	void ShowJoinLoadout();
	void FinishJoinLoadout();
	void ConfirmJoinLoadout();
	bool IsJoinLoadout() const;
	void ShowLoadingScreen(const FString& Message = TEXT("LOADING..."));
	void HideLoadingScreen();
	bool IsShowingLoading() const;
	void NotifyLocalMapReady();
	void OnMatchUnlocked();
	int32 CountSessionPlayers() const;
	void StartMatchFromLobby();
	void AuthorityStartMatchFromLobby();
	bool IsLocalLobbyLeader() const;
	void LeaveLobby();
	void RequestLobbyTeam(int32 Team);
	int32 GetPendingTeamId() const;
	void SetSelectedHeroIndex(int32 Index);
	int32 GetSelectedHeroIndex() const;
	int32 GetHeroChoiceCount() const;
	FString GetHeroDisplayName(int32 Index) const;
	TSubclassOf<class AMobaBaseCharacter> GetHeroClassAt(int32 Index) const;
	void ApplyLocalHeroChoice();
	int32 TakeCachedLobbyHero(const APlayerState* PS);
	void SetSimulatedPingRange(int32 MinMs, int32 MaxMs);
	int32 GetSimulatedPingMin() const;
	int32 GetSimulatedPingMax() const;
	void ApplySimulatedPing();
	int32 TakeCachedLobbyTeam(const APlayerState* PS);
	void ShowSettings();
	void HideSettings();
	void ToggleSettings();
	void QuitGame();

	float GetMasterVolume() const { return MasterVolume; }
	void SetMasterVolume(float Volume);
	void SetGraphicsQuality(int32 Level);
	int32 GetGraphicsQuality() const;

	virtual void LoadComplete(const float LoadTime, const FString& MapName) override;

	UMobaSessionSubsystem* GetSession() const;
	UMobaFrontEndSubsystem* GetFrontEnd() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Moba")
	FName ArenaMap = TEXT("/Game/Moba/Maps/MobaTestMap");

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Moba")
	FName MenuMap = TEXT("/Game/Moba/Maps/MobaMenu");

protected:
	void ApplySavedGraphics();

	UPROPERTY(config)
	float MasterVolume = 0.5f;
};
