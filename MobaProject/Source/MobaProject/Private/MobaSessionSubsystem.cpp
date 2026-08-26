#include "MobaSessionSubsystem.h"
#include "AMobaPlayerState.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "MobaBaseCharacter.h"
#include "MobaFrontEndSubsystem.h"
#include "MobaGameInstance.h"
#include "MobaGameMode.h"
#include "MobaVictoryManager.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "HAL/PlatformTime.h"
#include "Containers/Ticker.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "TimerManager.h"

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

void UMobaSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &UMobaSessionSubsystem::OnEngineNetworkFailure);
		GEngine->OnTravelFailure().AddUObject(this, &UMobaSessionSubsystem::OnEngineTravelFailure);
	}
}

void UMobaSessionSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	ClearJoinTimers();
	Super::Deinitialize();
}

UMobaGameInstance* UMobaSessionSubsystem::GetMobaGI() const
{
	return Cast<UMobaGameInstance>(GetGameInstance());
}

UMobaFrontEndSubsystem* UMobaSessionSubsystem::GetFrontEnd() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UMobaFrontEndSubsystem>() : nullptr;
}

FName UMobaSessionSubsystem::GetArenaMap() const
{
	const UMobaGameInstance* GI = GetMobaGI();
	return GI ? GI->ArenaMap : FName(TEXT("/Game/Moba/Maps/MobaTestMap"));
}

FName UMobaSessionSubsystem::GetMenuMap() const
{
	const UMobaGameInstance* GI = GetMobaGI();
	return GI ? GI->MenuMap : FName(TEXT("/Game/Moba/Maps/MobaMenu"));
}

bool UMobaSessionSubsystem::IsMenuMap(const FString& MapName) const
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

bool UMobaSessionSubsystem::IsInLobbyNet() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const ENetMode Mode = World->GetNetMode();
	return Mode == NM_ListenServer || Mode == NM_Client;
}

bool UMobaSessionSubsystem::ShouldShowLobby() const
{
	if (bJoinAborted || !bLobbySession)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	if (!World || !IsMenuMap(World->GetMapName()))
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

void UMobaSessionSubsystem::CollectLobbyKeys(const APlayerState* PS, TArray<FString>& OutKeys)
{
	OutKeys.Reset();
	if (!PS)
	{
		return;
	}
	OutKeys.AddUnique(FString::Printf(TEXT("P%d"), PS->GetPlayerId()));
	const FString Name = PS->GetPlayerName();
	if (!Name.IsEmpty())
	{
		OutKeys.AddUnique(FString::Printf(TEXT("N%s"), *Name));
	}
}

void UMobaSessionSubsystem::CacheLobbyTeams()
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
		int32 Hero = MobaPS->HeroIndex;
		const AController* Owner = MobaPS->GetOwningController();
		if (Owner && Owner->IsLocalController())
		{
			Hero = SelectedHeroIndex;
		}
		TArray<FString> Keys;
		CollectLobbyKeys(MobaPS, Keys);
		for (const FString& Key : Keys)
		{
			if (MobaPS->TeamID == 1 || MobaPS->TeamID == 2)
			{
				LobbyTeamByPlayer.Add(Key, MobaPS->TeamID);
			}
			LobbyHeroByPlayer.Add(Key, Hero);
		}
	}
}

int32 UMobaSessionSubsystem::TakeCachedLobbyTeam(const APlayerState* PS)
{
	TArray<FString> Keys;
	CollectLobbyKeys(PS, Keys);
	int32 Team = 0;
	for (const FString& Key : Keys)
	{
		if (const int32* Found = LobbyTeamByPlayer.Find(Key))
		{
			Team = *Found;
			break;
		}
	}
	for (const FString& Key : Keys)
	{
		LobbyTeamByPlayer.Remove(Key);
	}
	return Team;
}

int32 UMobaSessionSubsystem::TakeCachedLobbyHero(const APlayerState* PS)
{
	TArray<FString> Keys;
	CollectLobbyKeys(PS, Keys);
	int32 Index = INDEX_NONE;
	for (const FString& Key : Keys)
	{
		if (const int32* Found = LobbyHeroByPlayer.Find(Key))
		{
			Index = *Found;
			break;
		}
	}
	for (const FString& Key : Keys)
	{
		LobbyHeroByPlayer.Remove(Key);
	}
	return Index;
}

int32 UMobaSessionSubsystem::GetHeroChoiceCount() const
{
	return 2;
}

FString UMobaSessionSubsystem::GetHeroDisplayName(int32 Index) const
{
	return (Index == 1) ? TEXT("Mage") : TEXT("Brawler");
}

TSubclassOf<AMobaBaseCharacter> UMobaSessionSubsystem::GetHeroClassAt(int32 Index) const
{
	const TCHAR* Path = (Index == 1)
		? TEXT("/Game/Moba/BP_Mage.BP_Mage_C")
		: TEXT("/Game/Moba/BP_Brawler.BP_Brawler_C");
	return LoadClass<AMobaBaseCharacter>(nullptr, Path);
}

void UMobaSessionSubsystem::SetSelectedHeroIndex(int32 Index)
{
	SelectedHeroIndex = FMath::Clamp(Index, 0, GetHeroChoiceCount() - 1);
	ApplyLocalHeroChoice();
}

void UMobaSessionSubsystem::ApplyLocalHeroChoice()
{
	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
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

void UMobaSessionSubsystem::RequestLobbyTeam(int32 Team)
{
	if (Team != 1 && Team != 2)
	{
		return;
	}
	PendingTeamId = Team;
	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
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

int32 UMobaSessionSubsystem::CountSessionPlayers() const
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

void UMobaSessionSubsystem::NotifyLocalMapReady()
{
	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	AMobaPlayerState* PS = PC ? PC->GetPlayerState<AMobaPlayerState>() : nullptr;
	UWorld* World = GetWorld();
	if (PS && PS->IsMatchUnlocked())
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(MapReadyTimer);
		}
		return;
	}
	if (!PS)
	{
		if (World)
		{
			World->GetTimerManager().SetTimer(
				MapReadyTimer,
				this,
				&UMobaSessionSubsystem::NotifyLocalMapReadyRetry,
				0.15f,
				false);
		}
		return;
	}
	if (PS->HasAuthority())
	{
		PS->MarkMapLoaded();
	}
	else
	{
		PS->ServerNotifyMapLoaded();
	}
	if (World && !PS->IsMatchUnlocked())
	{
		World->GetTimerManager().SetTimer(
			MapReadyTimer,
			this,
			&UMobaSessionSubsystem::NotifyLocalMapReadyRetry,
			0.25f,
			false);
	}
}

void UMobaSessionSubsystem::NotifyLocalMapReadyRetry()
{
	NotifyLocalMapReady();
}

void UMobaSessionSubsystem::OnMatchUnlocked()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MapReadyTimer);
	}
	if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
	{
		Front->HideLoadingScreen();
		if (ShouldShowJoinLoadout())
		{
			Front->ShowJoinLoadout();
			return;
		}
		Front->ReleaseMenuInput();
	}
	UWorld* World = GetWorld();
	if (World && !IsMenuMap(World->GetMapName()))
	{
		if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
		{
			Front->ReleaseMenuInput();
		}
	}
}

bool UMobaSessionSubsystem::ShouldShowJoinLoadout() const
{
	const UWorld* World = GetWorld();
	if (!World || IsMenuMap(World->GetMapName()))
	{
		return false;
	}
	if (World->GetNetMode() == NM_ListenServer)
	{
		return false;
	}
	const UGameInstance* GI = GetGameInstance();
	const APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return false;
	}
	if (PC->GetPawn())
	{
		return false;
	}
	const AMobaPlayerState* PS = PC->GetPlayerState<AMobaPlayerState>();
	if (PS && PS->IsAwaitingLoadout())
	{
		return true;
	}
	return PS && PS->IsMatchUnlocked();
}

void UMobaSessionSubsystem::ConfirmJoinLoadout()
{
	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	AMobaPlayerState* PS = PC ? PC->GetPlayerState<AMobaPlayerState>() : nullptr;
	if (!PS)
	{
		return;
	}
	int32 Team = PendingTeamId;
	if (Team != 1 && Team != 2)
	{
		Team = (PS->TeamID == 1 || PS->TeamID == 2) ? PS->TeamID : 1;
	}
	PS->ServerSetHeroIndex(SelectedHeroIndex);
	PS->ServerConfirmLoadout(SelectedHeroIndex, Team);
}

void UMobaSessionSubsystem::StartMatchFromLobby()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (World->GetNetMode() == NM_Client)
	{
		UGameInstance* GI = GetGameInstance();
		APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
		AMobaPlayerState* PS = PC ? PC->GetPlayerState<AMobaPlayerState>() : nullptr;
		if (PS && PS->IsLobbyLeader())
		{
			PS->ServerStartMatchFromLobby();
			if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
			{
				Front->ShowLoadingScreen(TEXT("LOADING..."));
			}
		}
		return;
	}
	AuthorityStartMatchFromLobby();
}

void UMobaSessionSubsystem::AuthorityStartMatchFromLobby()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
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
	if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
	{
		Front->ShowLoadingScreen(TEXT("LOADING..."));
	}
	const int32 WaitPlayers = FMath::Max(1, CountSessionPlayers());
	FString Url = FString::Printf(
		TEXT("%s?game=/Script/MobaProject.MobaGameMode?WaitPlayers=%d"),
		*GetArenaMap().ToString(),
		WaitPlayers);
	if (World->GetNetMode() == NM_ListenServer)
	{
		Url += TEXT("?listen");
	}
	World->ServerTravel(Url);
}

bool UMobaSessionSubsystem::IsLocalLobbyLeader() const
{
	const UWorld* World = GetWorld();
	if (World && World->GetNetMode() == NM_ListenServer)
	{
		return true;
	}
	const UGameInstance* GI = GetGameInstance();
	const APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	const AMobaPlayerState* PS = PC ? PC->GetPlayerState<AMobaPlayerState>() : nullptr;
	return PS && PS->IsLobbyLeader();
}

void UMobaSessionSubsystem::LeaveLobby()
{
	PendingTeamId = 0;
	LobbyTeamByPlayer.Reset();
	LobbyHeroByPlayer.Reset();
	bLobbySession = false;
	bJoinLoadout = false;
	bAttemptingJoin = false;
	ClearJoinTimers();
	IgnoreNetFailUntil = FPlatformTime::Seconds() + 1.0;
	if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
	{
		Front->ShowLoadingScreen(TEXT("LOADING..."));
		Front->HideLobby();
	}
	if (UWorld* World = GetWorld())
	{
		if (World->GetNetMode() == NM_Client)
		{
			AbortJoinConnection();
		}
	}
	if (UMobaGameInstance* GI = GetMobaGI())
	{
		UGameplayStatics::OpenLevel(GI, GetMenuMap());
	}
}

void UMobaSessionSubsystem::HostGame()
{
	if (IsRunningClientOnly() || IsRunningDedicatedServer())
	{
		return;
	}

	PendingTeamId = 0;
	LobbyTeamByPlayer.Reset();
	LobbyHeroByPlayer.Reset();
	bJoinAborted = false;
	bAttemptingJoin = false;

	UMobaFrontEndSubsystem* Front = GetFrontEnd();
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
				if (Front)
				{
					Front->HideLoadingScreen();
					Front->HideMenu();
					Front->ShowLobby();
				}
				return;
			}
			JoinErrorMessage = TEXT("Could not host");
			if (Front)
			{
				Front->ShowMenu();
			}
			return;
		}
		if (World->GetNetMode() == NM_ListenServer)
		{
			bLobbySession = true;
			if (Front)
			{
				Front->HideLoadingScreen();
				Front->HideMenu();
				Front->ShowLobby();
			}
			return;
		}
	}

	bLobbySession = true;
	if (Front)
	{
		Front->ShowLoadingScreen(TEXT("LOADING..."));
		Front->HideMenu();
	}
	if (UMobaGameInstance* GI = GetMobaGI())
	{
		UGameplayStatics::OpenLevel(GI, GetMenuMap(), true, TEXT("listen"));
	}
}

void UMobaSessionSubsystem::BroadcastLoading(bool bPlayAgain)
{
	if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
	{
		Front->ShowLoadingScreen(bPlayAgain ? TEXT("LOADING...") : TEXT("RETURNING TO MENU..."));
	}
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

void UMobaSessionSubsystem::DoRestartTravel()
{
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_Client)
	{
		FString Url = FString::Printf(
			TEXT("%s?game=/Script/MobaProject.MobaGameMode?WaitPlayers=%d"),
			*GetArenaMap().ToString(),
			FMath::Max(1, CountSessionPlayers()));
		if (World->GetNetMode() == NM_ListenServer)
		{
			Url += TEXT("?listen");
		}
		World->ServerTravel(Url);
		return;
	}
	HostGame();
}

void UMobaSessionSubsystem::SendClientsToMenu()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	const FString MenuUrl = GetMenuMap().ToString();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && !PC->IsLocalController())
		{
			PC->ClientTravel(MenuUrl, TRAVEL_Absolute);
		}
	}
}

void UMobaSessionSubsystem::DoMenuTravel()
{
	if (UMobaGameInstance* GI = GetMobaGI())
	{
		UGameplayStatics::OpenLevel(GI, GetMenuMap());
	}
}

void UMobaSessionSubsystem::RestartMatch()
{
	BroadcastLoading(true);
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_Client)
	{
		World->GetTimerManager().SetTimer(
			TravelTimer,
			this,
			&UMobaSessionSubsystem::DoRestartTravel,
			0.2f,
			false);
		return;
	}
	HostGame();
}

void UMobaSessionSubsystem::ReturnToMenu()
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
			&UMobaSessionSubsystem::DoMenuTravel,
			0.25f,
			false);
		return;
	}
	if (UMobaGameInstance* GI = GetMobaGI())
	{
		UGameplayStatics::OpenLevel(GI, GetMenuMap());
	}
}

void UMobaSessionSubsystem::JoinGame(const FString& Address)
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

	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}

	UMobaFrontEndSubsystem* Front = GetFrontEnd();
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() == NM_ListenServer)
	{
		JoinErrorMessage = TEXT("Already hosting");
		if (Front)
		{
			Front->HideLoadingScreen();
			Front->ShowMenu();
		}
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
			if (Front)
			{
				Front->HideLoadingScreen();
				Front->ShowMenu();
			}
			return;
		}
	}

	ClearJoinTimers();
	PendingJoinUrl = Url;
	bJoinAborted = false;
	bLobbySession = false;
	JoinErrorMessage.Reset();
	if (Front)
	{
		Front->ShowLoadingScreen(TEXT("CONNECTING..."));
		Front->HideMenu();
	}

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
				&UMobaSessionSubsystem::BeginJoinTravel,
				0.2f,
				false);
			return;
		}
	}

	BeginJoinTravel();
}

void UMobaSessionSubsystem::HandleLoadComplete(const FString& MapName)
{
	UMobaFrontEndSubsystem* Front = GetFrontEnd();
	if (bJoinAborted)
	{
		if (IsInLobbyNet())
		{
			AbortJoinConnection();
			if (UMobaGameInstance* GI = GetMobaGI())
			{
				UGameplayStatics::OpenLevel(GI, GetMenuMap());
			}
			return;
		}
		bJoinAborted = false;
		bAttemptingJoin = false;
		ClearJoinTimers();
		if (Front)
		{
			Front->ShowMenu();
		}
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
			if (Front)
			{
				Front->ShowLoadingScreen(TEXT("CONNECTING..."), false);
			}
			return;
		}

		UWorld* World = GetWorld();
		if (World && World->GetNetMode() != NM_ListenServer)
		{
			bLobbySession = false;
		}

		if (Front)
		{
			if (ShouldShowLobby())
			{
				Front->ShowLobby();
			}
			else
			{
				Front->ShowMenu();
			}
			Front->RestoreUiPointerIfNeeded();
		}
		ApplySimulatedPing();
		EnsurePingTimer();
		return;
	}

	bAttemptingJoin = false;
	ClearJoinTimers();
	if (UWorld* ArenaWorld = GetWorld())
	{
		// Auth GameMode exists only on the server. Clients must still send map-loaded.
		if (ArenaWorld->GetNetMode() != NM_Client
			&& !ArenaWorld->GetAuthGameMode<AMobaGameMode>())
		{
			if (Front)
			{
				Front->HideLoadingScreen();
				Front->ReleaseMenuInput();
			}
			ApplySimulatedPing();
			EnsurePingTimer();
			return;
		}
	}
	NotifyLocalMapReady();
	if (ShouldShowJoinLoadout())
	{
		if (Front)
		{
			Front->ShowJoinLoadout();
		}
	}
	else if (Front)
	{
		Front->HideLobby();
		bool bUnlocked = false;
		bool bHasPawn = false;
		if (const UGameInstance* GI = GetGameInstance())
		{
			if (const APlayerController* PC = GI->GetFirstLocalPlayerController())
			{
				bHasPawn = PC->GetPawn() != nullptr;
				if (const AMobaPlayerState* PS = PC->GetPlayerState<AMobaPlayerState>())
				{
					bUnlocked = PS->IsMatchUnlocked();
				}
			}
		}
		if (bUnlocked || bHasPawn)
		{
			OnMatchUnlocked();
		}
		else
		{
			Front->ShowLoadingScreen(TEXT("WAITING FOR PLAYERS..."), false, false);
		}
	}
	ApplySimulatedPing();
	EnsurePingTimer();
}

void UMobaSessionSubsystem::SetSimulatedPingRange(int32 MinMs, int32 MaxMs)
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

void UMobaSessionSubsystem::ApplySimulatedPing()
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

void UMobaSessionSubsystem::ClearJoinTimers()
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

void UMobaSessionSubsystem::EnsurePingTimer()
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
			&UMobaSessionSubsystem::ApplySimulatedPing,
			0.5f,
			true);
		ApplySimulatedPing();
		return;
	}
	World->GetTimerManager().ClearTimer(PingApplyTimer);
}

bool UMobaSessionSubsystem::TryFinishJoin() const
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

void UMobaSessionSubsystem::EnterLobbyFromJoin()
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
	if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
	{
		Front->HideLoadingScreen();
		Front->ShowLobby();
	}
	ApplySimulatedPing();
	EnsurePingTimer();
}

void UMobaSessionSubsystem::BeginJoinTravel()
{
	bJoinAborted = false;
	bAttemptingJoin = true;
	bLobbySession = true;

	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!PC || PendingJoinUrl.IsEmpty())
	{
		FailJoin(TEXT("No lobby found"));
		return;
	}
	if (UMobaFrontEndSubsystem* Front = GetFrontEnd())
	{
		Front->ShowLoadingScreen(TEXT("CONNECTING..."));
		Front->HideMenu();
	}

	JoinGiveUpTime = FPlatformTime::Seconds() + 8.0;
	if (!JoinPollTicker.IsValid())
	{
		JoinPollTicker = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UMobaSessionSubsystem::TickJoinPoll),
			0.15f);
	}

	PC->ClientTravel(PendingJoinUrl, TRAVEL_Absolute, false);
	IgnoreNetFailUntil = FPlatformTime::Seconds() + 0.35;
}

bool UMobaSessionSubsystem::TickJoinPoll(float DeltaTime)
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

void UMobaSessionSubsystem::PollJoin()
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

void UMobaSessionSubsystem::OnEngineNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
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

void UMobaSessionSubsystem::OnEngineTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
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

void UMobaSessionSubsystem::OnJoinTimeout()
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

void UMobaSessionSubsystem::AbortJoinConnection()
{
	UWorld* World = GetWorld();
	UGameInstance* GI = GetGameInstance();
	if (!World && GI)
	{
		if (FWorldContext* Context = GI->GetWorldContext())
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

void UMobaSessionSubsystem::FailJoin(const FString& Message)
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
	UMobaFrontEndSubsystem* Front = GetFrontEnd();
	if (Front)
	{
		Front->HideLoadingScreen();
	}

	UWorld* World = GetWorld();
	if (World && IsMenuMap(World->GetMapName()))
	{
		bJoinAborted = false;
		if (Front)
		{
			Front->ShowMenu();
		}
		return;
	}
	if (UMobaGameInstance* GI = GetMobaGI())
	{
		UGameplayStatics::OpenLevel(GI, GetMenuMap());
	}
}

bool UMobaSessionSubsystem::ConsumeJoinError(FString& OutMessage)
{
	OutMessage = JoinErrorMessage;
	JoinErrorMessage.Reset();
	return !OutMessage.IsEmpty();
}
