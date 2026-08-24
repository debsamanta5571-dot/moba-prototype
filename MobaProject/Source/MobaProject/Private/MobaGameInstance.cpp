#include "MobaGameInstance.h"
#include "AMobaPlayerState.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "MobaLoadingWidget.h"
#include "MobaLobbyWidget.h"
#include "MobaMenuWidget.h"
#include "MobaSettingsWidget.h"
#include "MobaBaseCharacter.h"
#include "MobaVictoryManager.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Scalability.h"
#include "HAL/PlatformTime.h"
#include "Containers/Ticker.h"
#include "MoviePlayer.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

namespace
{
	bool IsLoopbackHost(const FString& Host)
	{
		return Host.Equals(TEXT("127.0.0.1"))
			|| Host.Equals(TEXT("localhost"), ESearchCase::IgnoreCase)
			|| Host.Equals(TEXT("::1"));
	}

	void ParseJoinHostPort(const FString& Url, FString& OutHost, int32& OutPort)
	{
		OutHost = Url;
		OutPort = 7777;
		int32 Colon = INDEX_NONE;
		if (OutHost.FindLastChar(TEXT(':'), Colon) && Colon > 0)
		{
			const FString PortStr = OutHost.Mid(Colon + 1);
			if (PortStr.IsNumeric())
			{
				OutPort = FCString::Atoi(*PortStr);
				OutHost.LeftInline(Colon);
			}
		}
	}

	bool IsLocalUdpPortFree(int32 Port)
	{
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (!Sockets)
		{
			return true;
		}
		FSocket* Socket = Sockets->CreateSocket(NAME_DGram, TEXT("MobaJoinProbe"), false);
		if (!Socket)
		{
			return true;
		}
		Socket->SetReuseAddr(false);
		TSharedRef<FInternetAddr> Addr = Sockets->CreateInternetAddr();
		Addr->SetAnyAddress();
		Addr->SetPort(Port);
		const bool bBound = Socket->Bind(*Addr);
		Sockets->DestroySocket(Socket);
		return bBound;
	}

	bool HasListenLobbyWorld()
	{
		if (!GEngine)
		{
			return false;
		}
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (const UWorld* World = Ctx.World())
			{
				if (World->GetNetMode() == NM_ListenServer)
				{
					return true;
				}
			}
		}
		return false;
	}

	bool HasStandaloneSiblingWorld(const UWorld* Self)
	{
		if (!GEngine)
		{
			return false;
		}
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			UWorld* World = Ctx.World();
			if (!World || World == Self)
			{
				continue;
			}
			const ENetMode Mode = World->GetNetMode();
			if (Mode == NM_Standalone || Mode == NM_Client)
			{
				return true;
			}
		}
		return false;
	}

	bool WorldHasClientConnection(const UWorld* World)
	{
		if (!World || World->GetNetMode() != NM_Client)
		{
			return false;
		}
		const UNetDriver* Driver = World->GetNetDriver();
		const UNetConnection* Conn = Driver ? Driver->ServerConnection : nullptr;
		if (!Conn)
		{
			return false;
		}
		const EConnectionState State = Conn->GetConnectionState();
		return State == USOCK_Open || State == USOCK_Pending;
	}

	bool WorldHasOpenClientConnection(const UWorld* World)
	{
		if (!World || World->GetNetMode() != NM_Client)
		{
			return false;
		}
		const UNetDriver* Driver = World->GetNetDriver();
		const UNetConnection* Conn = Driver ? Driver->ServerConnection : nullptr;
		return Conn && Conn->GetConnectionState() == USOCK_Open;
	}
}

void UMobaGameInstance::Init()
{
	Super::Init();
	LoadConfig();
	MasterVolume = FMath::Clamp(MasterVolume, 0.f, 1.f);
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &UMobaGameInstance::HandlePreLoadMap);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &UMobaGameInstance::OnEngineNetworkFailure);
		GEngine->OnTravelFailure().AddUObject(this, &UMobaGameInstance::OnEngineTravelFailure);
		ApplySavedGraphics();
	}
}

void UMobaGameInstance::Shutdown()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	HideLoadingScreen();
	ClearJoinTimers();
	Super::Shutdown();
}

void UMobaGameInstance::SetMasterVolume(float Volume)
{
	MasterVolume = FMath::Clamp(Volume, 0.f, 1.f);
	SaveConfig();
}

void UMobaGameInstance::ApplySettingsInput()
{
	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC || !IsValid(SettingsWidget))
	{
		return;
	}

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(SettingsWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;
	PC->FlushPressedKeys();
}

void UMobaGameInstance::ShowSettings()
{
	if (IsShowingLoading())
	{
		return;
	}

	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC)
	{
		return;
	}

	if (IsValid(SettingsWidget))
	{
		SettingsWidget->RemoveFromParent();
	}
	SettingsWidget = CreateWidget<UMobaSettingsWidget>(PC, UMobaSettingsWidget::StaticClass());
	if (!SettingsWidget)
	{
		return;
	}

	SettingsWidget->PlaceInViewport();
	ApplySettingsInput();
}

void UMobaGameInstance::HideSettings()
{
	if (IsValid(SettingsWidget))
	{
		SettingsWidget->RemoveFromParent();
	}
	SettingsWidget = nullptr;

	UWorld* World = GetWorld();
	if (World && IsMenuMap(World->GetMapName()))
	{
		if (ShouldShowLobby())
		{
			ShowLobby();
		}
		else
		{
			ShowMenu();
		}
		return;
	}
	ReleaseMenuInput();
}

void UMobaGameInstance::ToggleSettings()
{
	if (IsShowingLoading())
	{
		return;
	}
	if (IsValid(SettingsWidget) && SettingsWidget->IsInViewport())
	{
		HideSettings();
		return;
	}
	ShowSettings();
}

void UMobaGameInstance::ReleaseMenuInput()
{
	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC)
	{
		return;
	}

	PC->SetInputMode(FInputModeGameOnly());
	PC->bShowMouseCursor = false;
	PC->FlushPressedKeys();
}

void UMobaGameInstance::HideMenu()
{
	if (IsValid(SettingsWidget))
	{
		SettingsWidget->RemoveFromParent();
	}
	SettingsWidget = nullptr;
	if (IsValid(MenuWidget))
	{
		MenuWidget->RemoveFromParent();
	}
	MenuWidget = nullptr;
	HideLobby();
	ReleaseMenuInput();
}

void UMobaGameInstance::HideLobby()
{
	if (IsValid(LobbyWidget))
	{
		LobbyWidget->RemoveFromParent();
	}
	LobbyWidget = nullptr;
}

bool UMobaGameInstance::IsInLobbyNet() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const ENetMode Mode = World->GetNetMode();
	return Mode == NM_ListenServer || Mode == NM_Client;
}

bool UMobaGameInstance::ShouldShowLobby() const
{
	if (bJoinAborted || !bLobbySession)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const ENetMode Mode = World->GetNetMode();
	if (Mode == NM_ListenServer)
	{
		return true;
	}
	return Mode == NM_Client && WorldHasClientConnection(World);
}

FString UMobaGameInstance::LobbyPlayerKey(const APlayerState* PS)
{
	if (!PS)
	{
		return FString();
	}
	FString Name = PS->GetPlayerName();
	if (Name.IsEmpty())
	{
		Name = FString::Printf(TEXT("Player%d"), PS->GetPlayerId());
	}
	return Name;
}

void UMobaGameInstance::CacheLobbyTeams()
{
	LobbyTeamByPlayer.Reset();
	LobbyHeroByPlayer.Reset();
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}
	for (APlayerState* PS : GS->PlayerArray)
	{
		const AMobaPlayerState* MobaPS = Cast<AMobaPlayerState>(PS);
		if (!MobaPS)
		{
			continue;
		}
		const FString Key = LobbyPlayerKey(MobaPS);
		if (Key.IsEmpty())
		{
			continue;
		}
		if (MobaPS->TeamID == 1 || MobaPS->TeamID == 2)
		{
			LobbyTeamByPlayer.Add(Key, MobaPS->TeamID);
		}
		LobbyHeroByPlayer.Add(Key, MobaPS->HeroIndex);
	}
}

int32 UMobaGameInstance::TakeCachedLobbyTeam(const APlayerState* PS)
{
	const FString Key = LobbyPlayerKey(PS);
	if (Key.IsEmpty())
	{
		return 0;
	}
	if (const int32* Found = LobbyTeamByPlayer.Find(Key))
	{
		const int32 Team = *Found;
		LobbyTeamByPlayer.Remove(Key);
		return Team;
	}
	return 0;
}

int32 UMobaGameInstance::TakeCachedLobbyHero(const APlayerState* PS)
{
	const FString Key = LobbyPlayerKey(PS);
	if (Key.IsEmpty())
	{
		return INDEX_NONE;
	}
	if (const int32* Found = LobbyHeroByPlayer.Find(Key))
	{
		const int32 Index = *Found;
		LobbyHeroByPlayer.Remove(Key);
		return Index;
	}
	return INDEX_NONE;
}

int32 UMobaGameInstance::GetHeroChoiceCount() const
{
	return 2;
}

FString UMobaGameInstance::GetHeroDisplayName(int32 Index) const
{
	return (Index == 1) ? TEXT("Mage") : TEXT("Brawler");
}

TSubclassOf<AMobaBaseCharacter> UMobaGameInstance::GetHeroClassAt(int32 Index) const
{
	const TCHAR* Path = (Index == 1)
		? TEXT("/Game/Moba/BP_Mage.BP_Mage_C")
		: TEXT("/Game/Moba/BP_Brawler.BP_Brawler_C");
	return LoadClass<AMobaBaseCharacter>(nullptr, Path);
}

void UMobaGameInstance::SetSelectedHeroIndex(int32 Index)
{
	SelectedHeroIndex = FMath::Clamp(Index, 0, GetHeroChoiceCount() - 1);
	ApplyLocalHeroChoice();
}

void UMobaGameInstance::ApplyLocalHeroChoice()
{
	APlayerController* PC = GetFirstLocalPlayerController();
	AMobaPlayerState* PS = PC ? PC->GetPlayerState<AMobaPlayerState>() : nullptr;
	if (!PS)
	{
		return;
	}
	PS->ServerSetHeroIndex(SelectedHeroIndex);
	if (PC->HasAuthority())
	{
		PS->HeroIndex = SelectedHeroIndex;
	}
}

void UMobaGameInstance::RequestLobbyTeam(int32 Team)
{
	if (Team != 1 && Team != 2)
	{
		return;
	}
	PendingTeamId = Team;
	APlayerController* PC = GetFirstLocalPlayerController();
	AMobaPlayerState* PS = PC ? PC->GetPlayerState<AMobaPlayerState>() : nullptr;
	if (PS)
	{
		PS->ServerSetTeam(Team);
		if (PC->HasAuthority())
		{
			PS->TeamID = Team;
		}
	}
}

void UMobaGameInstance::ApplyLobbyInput()
{
	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC || !IsValid(LobbyWidget))
	{
		return;
	}

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(LobbyWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;
	PC->FlushPressedKeys();
}

void UMobaGameInstance::ShowLobby()
{
	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC)
	{
		return;
	}

	if (IsValid(MenuWidget))
	{
		MenuWidget->RemoveFromParent();
	}
	MenuWidget = nullptr;
	if (IsValid(SettingsWidget))
	{
		SettingsWidget->RemoveFromParent();
	}
	SettingsWidget = nullptr;

	if (IsValid(LobbyWidget))
	{
		LobbyWidget->RemoveFromParent();
	}
	LobbyWidget = CreateWidget<UMobaLobbyWidget>(PC, UMobaLobbyWidget::StaticClass());
	if (!LobbyWidget)
	{
		return;
	}

	LobbyWidget->PlaceInViewport();
	ApplyLobbyInput();
	ApplyLocalHeroChoice();
}

void UMobaGameInstance::ShowLoadingScreen(const FString& Message)
{
	const FString Text = Message.IsEmpty() ? TEXT("LOADING...") : Message;
	bLoadingScreenQueued = true;

	if (IsValid(SettingsWidget))
	{
		SettingsWidget->RemoveFromParent();
	}
	SettingsWidget = nullptr;
	if (IsValid(MenuWidget))
	{
		MenuWidget->RemoveFromParent();
	}
	MenuWidget = nullptr;
	HideLobby();

	if (IsValid(LoadingWidget))
	{
		LoadingWidget->RemoveFromParent();
	}
	LoadingWidget = CreateWidget<UMobaLoadingWidget>(this, UMobaLoadingWidget::StaticClass());
	if (LoadingWidget)
	{
		LoadingWidget->SetMessage(Text);
		LoadingWidget->PlaceInViewport();
	}

	if (APlayerController* PC = GetFirstLocalPlayerController())
	{
		FInputModeUIOnly InputMode;
		if (LoadingWidget)
		{
			InputMode.SetWidgetToFocus(LoadingWidget->TakeWidget());
		}
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
		PC->FlushPressedKeys();
	}
}

bool UMobaGameInstance::IsShowingLoading() const
{
	return bLoadingScreenQueued || (IsValid(LoadingWidget) && LoadingWidget->IsInViewport());
}

int32 UMobaGameInstance::CountSessionPlayers() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		return 1;
	}
	int32 Count = 0;
	for (const APlayerState* PS : GS->PlayerArray)
	{
		if (PS && !PS->IsOnlyASpectator())
		{
			++Count;
		}
	}
	return FMath::Max(Count, 1);
}

void UMobaGameInstance::NotifyLocalMapReady()
{
	APlayerController* PC = GetFirstLocalPlayerController();
	AMobaPlayerState* PS = PC ? PC->GetPlayerState<AMobaPlayerState>() : nullptr;
	if (!PS)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				MapReadyTimer,
				this,
				&UMobaGameInstance::NotifyLocalMapReady,
				0.1f,
				false);
		}
		return;
	}
	if (PS->HasAuthority())
	{
		PS->MarkMapLoaded();
		return;
	}
	PS->ServerNotifyMapLoaded();
}

void UMobaGameInstance::OnMatchUnlocked()
{
	HideLoadingScreen();
	if (UWorld* World = GetWorld())
	{
		if (!IsMenuMap(World->GetMapName()))
		{
			ReleaseMenuInput();
		}
	}
}

void UMobaGameInstance::HideLoadingScreen()
{
	bLoadingScreenQueued = false;
	StopLoadingMovie();
	if (IsValid(LoadingWidget))
	{
		LoadingWidget->RemoveFromParent();
	}
	LoadingWidget = nullptr;
}

void UMobaGameInstance::HandlePreLoadMap(const FWorldContext& LoadedContext, const FString& MapName)
{
	const FWorldContext* Mine = GetWorldContext();
	if (!Mine || Mine != &LoadedContext)
	{
		return;
	}
	(void)MapName;
	ShowLoadingScreen(TEXT("LOADING..."));
}

void UMobaGameInstance::StopLoadingMovie()
{
#if !UE_SERVER
	if (GetMoviePlayer() && GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		GetMoviePlayer()->StopMovie();
	}
#endif
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
	return 0;
}

void UMobaGameInstance::ApplySavedGraphics()
{
	SetGraphicsQuality(GetGraphicsQuality());
}

void UMobaGameInstance::SetupMovieLoadingScreen(const FString& Message)
{
	(void)Message;
}

void UMobaGameInstance::StartMatchFromLobby()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_ListenServer)
	{
		return;
	}
	CacheLobbyTeams();
	if (const AGameStateBase* GS = World->GetGameState())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (AMobaPlayerState* MobaPS = Cast<AMobaPlayerState>(PS))
			{
				MobaPS->ClientShowLoading(TEXT("LOADING..."));
			}
		}
	}
	ShowLoadingScreen(TEXT("LOADING..."));
	const int32 WaitPlayers = FMath::Max(1, CountSessionPlayers());
	World->ServerTravel(FString::Printf(TEXT("%s?listen?WaitPlayers=%d"), *ArenaMap.ToString(), WaitPlayers));
}

void UMobaGameInstance::LeaveLobby()
{
	PendingTeamId = 0;
	LobbyTeamByPlayer.Reset();
	LobbyHeroByPlayer.Reset();
	bLobbySession = false;
	bAttemptingJoin = false;
	ClearJoinTimers();
	IgnoreNetFailUntil = FPlatformTime::Seconds() + 1.0;
	ShowLoadingScreen(TEXT("LOADING..."));
	HideLobby();
	if (UWorld* World = GetWorld())
	{
		if (World->GetNetMode() == NM_Client)
		{
			AbortJoinConnection();
		}
	}
	UGameplayStatics::OpenLevel(this, MenuMap);
}

void UMobaGameInstance::ShowMenu()
{
	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC)
	{
		return;
	}

	HideLobby();
	if (IsValid(MenuWidget))
	{
		MenuWidget->RemoveFromParent();
	}
	MenuWidget = CreateWidget<UMobaMenuWidget>(PC, UMobaMenuWidget::StaticClass());
	if (MenuWidget)
	{
		MenuWidget->AddToViewport(100);
	}
	if (!MenuWidget)
	{
		return;
	}

	MenuWidget->SetNotice(JoinErrorMessage);
	JoinErrorMessage.Reset();

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;
	PC->FlushPressedKeys();
	MenuWidget->FocusJoinAddress();
}

bool UMobaGameInstance::IsMenuMap(const FString& MapName) const
{
	if (MapName.Contains(TEXT("MobaMenu"), ESearchCase::IgnoreCase))
	{
		return true;
	}
	if (const UWorld* World = GetWorld())
	{
		if (World->GetMapName().Contains(TEXT("MobaMenu"), ESearchCase::IgnoreCase))
		{
			return true;
		}
		if (World->URL.Map.Contains(TEXT("MobaMenu"), ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

void UMobaGameInstance::HostGame()
{
	PendingTeamId = 0;
	LobbyTeamByPlayer.Reset();
	LobbyHeroByPlayer.Reset();
	bJoinAborted = false;
	bAttemptingJoin = false;

	UWorld* World = GetWorld();
	if (World && IsMenuMap(World->GetMapName()))
	{
		if (World->GetNetMode() == NM_Standalone)
		{
			FURL ListenURL = World->URL;
			ListenURL.AddOption(TEXT("Listen"));
			if (World->Listen(ListenURL))
			{
				World->URL.AddOption(TEXT("Listen"));
				bLobbySession = true;
				HideMenu();
				ShowLobby();
				return;
			}
			JoinErrorMessage = TEXT("Could not host");
			ShowMenu();
			return;
		}
		if (World->GetNetMode() == NM_ListenServer)
		{
			bLobbySession = true;
			HideMenu();
			ShowLobby();
			return;
		}
	}

	bLobbySession = true;
	ShowLoadingScreen(TEXT("LOADING..."));
	HideMenu();
	UGameplayStatics::OpenLevel(this, MenuMap, true, TEXT("listen"));
}

void UMobaGameInstance::BroadcastLoading(bool bPlayAgain)
{
	ShowLoadingScreen(bPlayAgain ? TEXT("LOADING...") : TEXT("RETURNING TO MENU..."));
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<AMobaVictoryManager> It(World); It; ++It)
	{
		It->NotifyLoading(bPlayAgain);
		break;
	}
}

void UMobaGameInstance::DoRestartTravel()
{
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_Client)
	{
		World->ServerTravel(FString::Printf(TEXT("%s?listen?WaitPlayers=%d"), *ArenaMap.ToString(), FMath::Max(1, CountSessionPlayers())));
		return;
	}
	HostGame();
}

void UMobaGameInstance::SendClientsToMenu()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	const FString MenuUrl = MenuMap.ToString();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && !PC->IsLocalController())
		{
			PC->ClientTravel(MenuUrl, TRAVEL_Absolute);
		}
	}
}

void UMobaGameInstance::DoMenuTravel()
{
	UGameplayStatics::OpenLevel(this, MenuMap);
}

void UMobaGameInstance::RestartMatch()
{
	BroadcastLoading(true);
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_Client)
	{
		World->GetTimerManager().SetTimer(
			TravelTimer,
			this,
			&UMobaGameInstance::DoRestartTravel,
			0.2f,
			false);
		return;
	}
	HostGame();
}

void UMobaGameInstance::ReturnToMenu()
{
	bLobbySession = false;
	bAttemptingJoin = false;
	ClearJoinTimers();
	BroadcastLoading(false);
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_Client)
	{
		SendClientsToMenu();
		World->GetTimerManager().SetTimer(
			TravelTimer,
			this,
			&UMobaGameInstance::DoMenuTravel,
			0.25f,
			false);
		return;
	}
	UGameplayStatics::OpenLevel(this, MenuMap);
}

void UMobaGameInstance::JoinGame(const FString& Address)
{
	FString Url = Address.TrimStartAndEnd();
	if (Url.IsEmpty())
	{
		Url = TEXT("127.0.0.1");
	}
	if (!Url.Contains(TEXT(":")))
	{
		Url += TEXT(":7777");
	}

	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World && World->GetNetMode() == NM_ListenServer)
	{
		JoinErrorMessage = TEXT("Already hosting");
		HideLoadingScreen();
		ShowMenu();
		return;
	}

	FString Host;
	int32 Port = 7777;
	ParseJoinHostPort(Url, Host, Port);
	const bool bLoopback = IsLoopbackHost(Host);
	if (bLoopback && !HasListenLobbyWorld())
	{
		if (HasStandaloneSiblingWorld(GetWorld()) || IsLocalUdpPortFree(Port))
		{
			JoinErrorMessage = TEXT("No lobby found");
			HideLoadingScreen();
			ShowMenu();
			return;
		}
	}

	ClearJoinTimers();
	PendingJoinUrl = Url;
	bJoinAborted = false;
	bLobbySession = false;
	JoinErrorMessage.Reset();
	ShowLoadingScreen(TEXT("CONNECTING..."));
	HideMenu();

	const bool bStaleClient = World && World->GetNetMode() == NM_Client;
	if (bStaleClient || bAttemptingJoin)
	{
		bAttemptingJoin = false;
		IgnoreNetFailUntil = FPlatformTime::Seconds() + 1.0;
		AbortJoinConnection();
		if (UWorld* TimerWorld = GetWorld())
		{
			TimerWorld->GetTimerManager().SetTimer(
				JoinTravelTimer,
				this,
				&UMobaGameInstance::BeginJoinTravel,
				0.2f,
				false);
			return;
		}
	}

	BeginJoinTravel();
}

void UMobaGameInstance::LoadComplete(const float LoadTime, const FString& MapName)
{
	Super::LoadComplete(LoadTime, MapName);
	HideLoadingScreen();
	ApplySavedGraphics();

	if (bJoinAborted)
	{
		if (IsInLobbyNet())
		{
			AbortJoinConnection();
			UGameplayStatics::OpenLevel(this, MenuMap);
			return;
		}
		bJoinAborted = false;
		bAttemptingJoin = false;
		ClearJoinTimers();
		ShowMenu();
		ApplySimulatedPing();
		return;
	}

	if (IsMenuMap(MapName))
	{
		if (bAttemptingJoin && TryFinishJoin())
		{
			EnterLobbyFromJoin();
			return;
		}
		if (bAttemptingJoin)
		{
			ShowLoadingScreen(TEXT("CONNECTING..."));
			return;
		}

		UWorld* World = GetWorld();
		if (World && World->GetNetMode() != NM_ListenServer)
		{
			bLobbySession = false;
		}

		if (ShouldShowLobby())
		{
			ShowLobby();
		}
		else
		{
			ShowMenu();
		}
		ApplySimulatedPing();
		EnsurePingTimer();
		return;
	}

	bAttemptingJoin = false;
	ClearJoinTimers();
	HideLobby();
	NotifyLocalMapReady();
	bool bUnlocked = false;
	if (const APlayerController* PC = GetFirstLocalPlayerController())
	{
		if (const AMobaPlayerState* PS = PC->GetPlayerState<AMobaPlayerState>())
		{
			bUnlocked = PS->IsMatchUnlocked();
		}
	}
	if (bUnlocked)
	{
		OnMatchUnlocked();
	}
	else
	{
		ShowLoadingScreen(TEXT("WAITING FOR PLAYERS..."));
	}
	ApplySimulatedPing();
	EnsurePingTimer();
}

void UMobaGameInstance::SetSimulatedPingRange(int32 MinMs, int32 MaxMs)
{
	SimulatedPingMinMs = FMath::Clamp(MinMs, 0, 300);
	SimulatedPingMaxMs = FMath::Clamp(MaxMs, 0, 300);
	if (SimulatedPingMinMs > SimulatedPingMaxMs)
	{
		Swap(SimulatedPingMinMs, SimulatedPingMaxMs);
	}
	ApplySimulatedPing();
	EnsurePingTimer();
}

void UMobaGameInstance::ApplySimulatedPing()
{
	UWorld* World = GetWorld();
	if (!GEngine || !World)
	{
		return;
	}
	if (World->GetNetMode() != NM_Client)
	{
		return;
	}

#if DO_ENABLE_NET_TEST
	FPacketSimulationSettings Settings;
	const int32 MinMs = FMath::Max(0, SimulatedPingMinMs);
	const int32 MaxMs = FMath::Max(MinMs, SimulatedPingMaxMs);
	if (MaxMs > 0)
	{
		const int32 OutMin = MinMs / 2;
		const int32 OutMax = MaxMs / 2;
		Settings.PktLag = 0;
		Settings.PktLagVariance = 0;
		Settings.PktLagMin = OutMin;
		Settings.PktLagMax = OutMax;
		Settings.PktIncomingLagMin = MinMs - OutMin;
		Settings.PktIncomingLagMax = MaxMs - OutMax;
	}
	if (FWorldContext* Ctx = GEngine->GetWorldContextFromWorld(World))
	{
		for (const FNamedNetDriver& Named : Ctx->ActiveNetDrivers)
		{
			if (Named.NetDriver)
			{
				Named.NetDriver->SetPacketSimulationSettings(Settings);
			}
		}
	}
#endif
}

void UMobaGameInstance::ClearJoinTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(JoinTimeoutTimer);
		World->GetTimerManager().ClearTimer(JoinPollTimer);
		World->GetTimerManager().ClearTimer(JoinTravelTimer);
	}
	if (JoinPollTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(JoinPollTicker);
		JoinPollTicker.Reset();
	}
}

void UMobaGameInstance::EnsurePingTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const bool bNeed = World->GetNetMode() == NM_Client
		&& FMath::Max(SimulatedPingMinMs, SimulatedPingMaxMs) > 0;
	if (bNeed)
	{
		World->GetTimerManager().SetTimer(
			PingApplyTimer,
			this,
			&UMobaGameInstance::ApplySimulatedPing,
			0.5f,
			true);
		ApplySimulatedPing();
		return;
	}
	World->GetTimerManager().ClearTimer(PingApplyTimer);
}

bool UMobaGameInstance::TryFinishJoin() const
{
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_Client)
	{
		return false;
	}
	if (WorldHasOpenClientConnection(World))
	{
		return true;
	}
	return WorldHasClientConnection(World) && World->GetGameState() != nullptr;
}

void UMobaGameInstance::EnterLobbyFromJoin()
{
	bAttemptingJoin = false;
	bJoinAborted = false;
	bLobbySession = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(JoinTimeoutTimer);
		World->GetTimerManager().ClearTimer(JoinPollTimer);
		World->GetTimerManager().ClearTimer(JoinTravelTimer);
	}
	HideLoadingScreen();
	ShowLobby();
	ApplySimulatedPing();
	EnsurePingTimer();
}

void UMobaGameInstance::BeginJoinTravel()
{
	bJoinAborted = false;
	bAttemptingJoin = true;
	bLobbySession = true;

	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC || PendingJoinUrl.IsEmpty())
	{
		FailJoin(TEXT("No lobby found"));
		return;
	}
	ShowLoadingScreen(TEXT("CONNECTING..."));
	HideMenu();

	JoinGiveUpTime = FPlatformTime::Seconds() + 8.0;
	if (!JoinPollTicker.IsValid())
	{
		JoinPollTicker = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UMobaGameInstance::TickJoinPoll),
			0.15f);
	}

	PC->ClientTravel(PendingJoinUrl, TRAVEL_Absolute, false);
	IgnoreNetFailUntil = FPlatformTime::Seconds() + 0.35;
}

bool UMobaGameInstance::TickJoinPoll(float DeltaTime)
{
	(void)DeltaTime;
	if (!bAttemptingJoin || bJoinAborted)
	{
		JoinPollTicker.Reset();
		return false;
	}
	if (TryFinishJoin())
	{
		EnterLobbyFromJoin();
		JoinPollTicker.Reset();
		return false;
	}
	if (FPlatformTime::Seconds() >= JoinGiveUpTime)
	{
		OnJoinTimeout();
		JoinPollTicker.Reset();
		return false;
	}
	return true;
}

void UMobaGameInstance::PollJoin()
{
	if (!bAttemptingJoin || bJoinAborted)
	{
		return;
	}
	if (TryFinishJoin())
	{
		EnterLobbyFromJoin();
	}
}

void UMobaGameInstance::OnEngineNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	(void)NetDriver;
	(void)FailureType;
	(void)ErrorString;
	if (World && GetWorld() && World != GetWorld())
	{
		return;
	}
	if (FPlatformTime::Seconds() < IgnoreNetFailUntil)
	{
		return;
	}
	if (bAttemptingJoin)
	{
		FailJoin(TEXT("No lobby found"));
	}
}

void UMobaGameInstance::OnEngineTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	(void)FailureType;
	(void)ErrorString;
	if (World && GetWorld() && World != GetWorld())
	{
		return;
	}
	if (FPlatformTime::Seconds() < IgnoreNetFailUntil)
	{
		return;
	}
	if (bAttemptingJoin)
	{
		FailJoin(TEXT("No lobby found"));
	}
}

void UMobaGameInstance::OnJoinTimeout()
{
	if (!bAttemptingJoin)
	{
		return;
	}
	if (TryFinishJoin())
	{
		EnterLobbyFromJoin();
		return;
	}
	FailJoin(TEXT("No lobby found"));
}

void UMobaGameInstance::AbortJoinConnection()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		if (FWorldContext* Context = GetWorldContext())
		{
			World = Context->World();
		}
	}
	if (GEngine && World)
	{
		GEngine->CancelPending(World);
		if (UNetDriver* NetDriver = World->GetNetDriver())
		{
			GEngine->DestroyNamedNetDriver(World, NetDriver->NetDriverName);
		}
	}
}

void UMobaGameInstance::FailJoin(const FString& Message)
{
	if (!bAttemptingJoin)
	{
		return;
	}
	bAttemptingJoin = false;
	bJoinAborted = true;
	bLobbySession = false;
	JoinErrorMessage = Message;
	ClearJoinTimers();
	IgnoreNetFailUntil = FPlatformTime::Seconds() + 1.0;
	AbortJoinConnection();
	HideLoadingScreen();

	UWorld* World = GetWorld();
	if (World && IsMenuMap(World->GetMapName()))
	{
		bJoinAborted = false;
		ShowMenu();
		return;
	}

	UGameplayStatics::OpenLevel(this, MenuMap);
}
