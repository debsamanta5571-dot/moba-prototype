#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MobaFrontEndSubsystem.generated.h"

class UMobaLoadingWidget;
class UMobaLobbyWidget;
class UMobaMenuWidget;
class UMobaSettingsWidget;
class UUserWidget;
struct FWorldContext;

// Menu, lobby, settings, and loading movie. Session owns host/join/travel.
UCLASS()
class MOBAPROJECT_API UMobaFrontEndSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void ShowMenu();
	void HideMenu();
	void ShowLobby();
	void HideLobby();
	void ShowJoinLoadout();
	void FinishJoinLoadout();
	void ShowLoadingScreen(const FString& Message = TEXT("LOADING..."), bool bPrepareMovie = true, bool bCaptureInput = true);
	void HideLoadingScreen();
	bool IsShowingLoading() const;
	void ShowSettings();
	void HideSettings();
	void ToggleSettings();
	void RestoreUiPointerIfNeeded();
	bool IsMenuUiActive() const;
	void ReleaseMenuInput();
	UMobaLobbyWidget* GetLobbyWidget() const { return LobbyWidget; }

protected:
	void ApplyUiPointer(UUserWidget* FocusWidget);
	void ApplySettingsInput();
	void ApplyLobbyInput();
	void ApplyMenuCamera();
	void SetupMovieLoadingScreen(const FString& Message);
	void StopLoadingMovie();
	void HandlePreLoadMap(const FWorldContext& LoadedContext, const FString& MapName);

	UPROPERTY()
	TObjectPtr<UMobaMenuWidget> MenuWidget;

	UPROPERTY()
	TObjectPtr<UMobaLobbyWidget> LobbyWidget;

	UPROPERTY()
	TObjectPtr<UMobaSettingsWidget> SettingsWidget;

	UPROPERTY()
	TObjectPtr<UMobaLoadingWidget> LoadingWidget;

	bool bLoadingScreenQueued = false;
};
