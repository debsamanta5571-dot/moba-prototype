#include "MobaGameInstance.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MobaBaseCharacter.h"
#include "MobaFrontEndSubsystem.h"
#include "MobaSessionSubsystem.h"
#include "Scalability.h"

UMobaSessionSubsystem* UMobaGameInstance::GetSession() const
{
	return GetSubsystem<UMobaSessionSubsystem>();
}

UMobaFrontEndSubsystem* UMobaGameInstance::GetFrontEnd() const
{
	return GetSubsystem<UMobaFrontEndSubsystem>();
}

void UMobaGameInstance::Init()
{
	Super::Init();
	LoadConfig();
	MasterVolume = FMath::Clamp(MasterVolume, 0.f, 1.f);
	if (GEngine)
	{
		ApplySavedGraphics();
	}
}

void UMobaGameInstance::Shutdown()
{
	Super::Shutdown();
}

void UMobaGameInstance::SetMasterVolume(float Volume)
{
	MasterVolume = FMath::Clamp(Volume, 0.f, 1.f);
	SaveConfig();
}

void UMobaGameInstance::SetGraphicsQuality(int32 Level)
{
	Level = FMath::Clamp(Level, 0, 3);
	Scalability::FQualityLevels Levels;
	Levels.SetFromSingleQualityLevel(Level);
	Scalability::SetQualityLevels(Levels, true);
	if (!GEngine)
	{
		return;
	}
	if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
	{
		Settings->SetOverallScalabilityLevel(Level);
		Settings->ApplyNonResolutionSettings();
		Settings->SaveSettings();
	}
}

int32 UMobaGameInstance::GetGraphicsQuality() const
{
	if (GEngine)
	{
		if (const UGameUserSettings* Settings = GEngine->GetGameUserSettings())
		{
			const int32 Level = Settings->GetOverallScalabilityLevel();
			if (Level >= 0)
			{
				return FMath::Clamp(Level, 0, 3);
			}
		}
	}
	return 2;
}

void UMobaGameInstance::ApplySavedGraphics()
{
	SetGraphicsQuality(GetGraphicsQuality());
}

void UMobaGameInstance::QuitGame()
{
	UKismetSystemLibrary::QuitGame(GetWorld(), GetFirstLocalPlayerController(), EQuitPreference::Quit, false);
}

void UMobaGameInstance::LoadComplete(const float LoadTime, const FString& MapName)
{
	Super::LoadComplete(LoadTime, MapName);
	if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
	{
		Front->HideLoadingScreen();
	}
	ApplySavedGraphics();
	if (UMobaSessionSubsystem* Session = GetSession())
	{
		Session->HandleLoadComplete(MapName);
	}
}

void UMobaGameInstance::HostGame()
{
	if (UMobaSessionSubsystem* Session = GetSession())
	{
		Session->HostGame();
	}
}

void UMobaGameInstance::JoinGame(const FString& Address)
{
	if (UMobaSessionSubsystem* Session = GetSession())
	{
		Session->JoinGame(Address);
	}
}

void UMobaGameInstance::RestartMatch()
{
	if (UMobaSessionSubsystem* Session = GetSession())
	{
		Session->RestartMatch();
	}
}

void UMobaGameInstance::ReturnToMenu()
{
	if (UMobaSessionSubsystem* Session = GetSession())
	{
		Session->ReturnToMenu();
	}
}

void UMobaGameInstance::ShowMenu()
{
	if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
	{
		Front->ShowMenu();
	}
}

void UMobaGameInstance::RestoreUiPointerIfNeeded()
{
	if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
	{
		Front->RestoreUiPointerIfNeeded();
	}
}

bool UMobaGameInstance::IsMenuUiActive() const
{
	const UMobaFrontEndSubsystem* Front = GetFrontEnd();
	return Front && Front->IsMenuUiActive();
}

void UMobaGameInstance::ShowLobby()
{
	if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
	{
		Front->ShowLobby();
	}
}

bool UMobaGameInstance::ShouldShowLobby() const
{
	const UMobaSessionSubsystem* Session = GetSession();
	return Session && Session->ShouldShowLobby();
}

bool UMobaGameInstance::ShouldShowJoinLoadout() const
{
	const UMobaSessionSubsystem* Session = GetSession();
	return Session && Session->ShouldShowJoinLoadout();
}

void UMobaGameInstance::ShowJoinLoadout()
{
	if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
	{
		Front->ShowJoinLoadout();
	}
}

void UMobaGameInstance::FinishJoinLoadout()
{
	if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
	{
		Front->FinishJoinLoadout();
	}
}

void UMobaGameInstance::ConfirmJoinLoadout()
{
	if (UMobaSessionSubsystem* Session = GetSession())
	{
		Session->ConfirmJoinLoadout();
	}
}

bool UMobaGameInstance::IsJoinLoadout() const
{
	const UMobaSessionSubsystem* Session = GetSession();
	return Session && Session->IsJoinLoadout();
}

void UMobaGameInstance::ShowLoadingScreen(const FString& Message)
{
	if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
	{
		Front->ShowLoadingScreen(Message);
	}
}

void UMobaGameInstance::HideLoadingScreen()
{
	if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
	{
		Front->HideLoadingScreen();
	}
}

bool UMobaGameInstance::IsShowingLoading() const
{
	const UMobaFrontEndSubsystem* Front = GetFrontEnd();
	return Front && Front->IsShowingLoading();
}

void UMobaGameInstance::NotifyLocalMapReady()
{
	if (UMobaSessionSubsystem* Session = GetSession())
	{
		Session->NotifyLocalMapReady();
	}
}

void UMobaGameInstance::OnMatchUnlocked()
{
	if (UMobaSessionSubsystem* Session = GetSession())
	{
		Session->OnMatchUnlocked();
	}
}

int32 UMobaGameInstance::CountSessionPlayers() const
{
	const UMobaSessionSubsystem* Session = GetSession();
	return Session ? Session->CountSessionPlayers() : 1;
}

void UMobaGameInstance::StartMatchFromLobby()
{
	if (UMobaSessionSubsystem* Session = GetSession())
	{
		Session->StartMatchFromLobby();
	}
}

void UMobaGameInstance::AuthorityStartMatchFromLobby()
{
	if (UMobaSessionSubsystem* Session = GetSession())
	{
		Session->AuthorityStartMatchFromLobby();
	}
}

bool UMobaGameInstance::IsLocalLobbyLeader() const
{
	const UMobaSessionSubsystem* Session = GetSession();
	return Session && Session->IsLocalLobbyLeader();
}

void UMobaGameInstance::LeaveLobby()
{
	if (UMobaSessionSubsystem* Session = GetSession())
	{
		Session->LeaveLobby();
	}
}

void UMobaGameInstance::RequestLobbyTeam(int32 Team)
{
	if (UMobaSessionSubsystem* Session = GetSession())
	{
		Session->RequestLobbyTeam(Team);
	}
}

int32 UMobaGameInstance::GetPendingTeamId() const
{
	const UMobaSessionSubsystem* Session = GetSession();
	return Session ? Session->GetPendingTeamId() : 0;
}

void UMobaGameInstance::SetSelectedHeroIndex(int32 Index)
{
	if (UMobaSessionSubsystem* Session = GetSession())
	{
		Session->SetSelectedHeroIndex(Index);
	}
}

int32 UMobaGameInstance::GetSelectedHeroIndex() const
{
	const UMobaSessionSubsystem* Session = GetSession();
	return Session ? Session->GetSelectedHeroIndex() : 0;
}

int32 UMobaGameInstance::GetHeroChoiceCount() const
{
	const UMobaSessionSubsystem* Session = GetSession();
	return Session ? Session->GetHeroChoiceCount() : 2;
}

FString UMobaGameInstance::GetHeroDisplayName(int32 Index) const
{
	const UMobaSessionSubsystem* Session = GetSession();
	return Session ? Session->GetHeroDisplayName(Index) : FString();
}

TSubclassOf<AMobaBaseCharacter> UMobaGameInstance::GetHeroClassAt(int32 Index) const
{
	UMobaSessionSubsystem* Session = GetSession();
	return Session ? Session->GetHeroClassAt(Index) : nullptr;
}

void UMobaGameInstance::ApplyLocalHeroChoice()
{
	if (UMobaSessionSubsystem* Session = GetSession())
	{
		Session->ApplyLocalHeroChoice();
	}
}

int32 UMobaGameInstance::TakeCachedLobbyHero(const APlayerState* PS)
{
	UMobaSessionSubsystem* Session = GetSession();
	return Session ? Session->TakeCachedLobbyHero(PS) : INDEX_NONE;
}

void UMobaGameInstance::SetSimulatedPingRange(int32 MinMs, int32 MaxMs)
{
	if (UMobaSessionSubsystem* Session = GetSession())
	{
		Session->SetSimulatedPingRange(MinMs, MaxMs);
	}
}

int32 UMobaGameInstance::GetSimulatedPingMin() const
{
	const UMobaSessionSubsystem* Session = GetSession();
	return Session ? Session->GetSimulatedPingMin() : 0;
}

int32 UMobaGameInstance::GetSimulatedPingMax() const
{
	const UMobaSessionSubsystem* Session = GetSession();
	return Session ? Session->GetSimulatedPingMax() : 0;
}

void UMobaGameInstance::ApplySimulatedPing()
{
	if (UMobaSessionSubsystem* Session = GetSession())
	{
		Session->ApplySimulatedPing();
	}
}

int32 UMobaGameInstance::TakeCachedLobbyTeam(const APlayerState* PS)
{
	UMobaSessionSubsystem* Session = GetSession();
	return Session ? Session->TakeCachedLobbyTeam(PS) : 0;
}

void UMobaGameInstance::ShowSettings()
{
	if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
	{
		Front->ShowSettings();
	}
}

void UMobaGameInstance::HideSettings()
{
	if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
	{
		Front->HideSettings();
	}
}

void UMobaGameInstance::ToggleSettings()
{
	if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
	{
		Front->ToggleSettings();
	}
}
