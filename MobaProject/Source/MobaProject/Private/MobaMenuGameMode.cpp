#include "MobaMenuGameMode.h"
#include "AMobaPlayerState.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "MobaGameInstance.h"

AMobaMenuGameMode::AMobaMenuGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerStateClass = AMobaPlayerState::StaticClass();
	HUDClass = nullptr;
	bUseSeamlessTravel = false;
}

void AMobaMenuGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	bUseSeamlessTravel = false;
}

void AMobaMenuGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (AMobaPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AMobaPlayerState>() : nullptr)
	{
		PS->AssignLobbyName();
	}
	AssignLobbyTeam(NewPlayer);
	if (NewPlayer && NewPlayer->IsLocalController())
	{
		if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
		{
			GI->ApplyLocalHeroChoice();
		}
	}
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	EnsureLobbyLeader();
}

void AMobaMenuGameMode::AssignLobbyTeam(AController* Player)
{
	AMobaPlayerState* PS = Player ? Player->GetPlayerState<AMobaPlayerState>() : nullptr;
	if (!PS)
	{
		return;
	}
	if (PS->TeamID == 1 || PS->TeamID == 2)
	{
		return;
	}
	if (Player && Player->IsLocalController())
	{
		if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
		{
			const int32 Pending = GI->GetPendingTeamId();
			if (Pending == 1 || Pending == 2)
			{
				PS->TeamID = Pending;
				return;
			}
		}
	}
	const int32 Team = (NextJoinTeam == 2) ? 2 : 1;
	NextJoinTeam = (Team == 1) ? 2 : 1;
	PS->TeamID = Team;
}

void AMobaMenuGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	EnsureLobbyLeader();
}

// Listen host is always leader. Dedicated has no host, so the first joiner is.
void AMobaMenuGameMode::EnsureLobbyLeader()
{
	UWorld* World = GetWorld();
	AGameStateBase* GS = GameState;
	if (!GS)
	{
		GS = World ? World->GetGameState() : nullptr;
	}
	if (!GS)
	{
		return;
	}

	AMobaPlayerState* Leader = nullptr;
	if (World && World->GetNetMode() == NM_ListenServer)
	{
		if (APlayerController* HostPC = World->GetFirstPlayerController())
		{
			if (HostPC->IsLocalController())
			{
				Leader = HostPC->GetPlayerState<AMobaPlayerState>();
			}
		}
	}
	if (!Leader)
	{
		int32 BestId = MAX_int32;
		for (APlayerState* PS : GS->PlayerArray)
		{
			AMobaPlayerState* MobaPS = Cast<AMobaPlayerState>(PS);
			if (!MobaPS || MobaPS->IsOnlyASpectator())
			{
				continue;
			}
			const int32 Id = MobaPS->GetPlayerId();
			if (!Leader || Id < BestId)
			{
				Leader = MobaPS;
				BestId = Id;
			}
		}
	}

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (AMobaPlayerState* MobaPS = Cast<AMobaPlayerState>(PS))
		{
			const bool bShouldLead = (MobaPS == Leader);
			if (MobaPS->bLobbyLeader != bShouldLead)
			{
				MobaPS->bLobbyLeader = bShouldLead;
			}
		}
	}
}

void AMobaMenuGameMode::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PC && PC->IsLocalController())
	{
		for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
		{
			PC->SetViewTarget(*It);
			break;
		}
	}

	if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
	{
		UWorld* World = GetWorld();
		const bool bMenuMap = World && World->GetMapName().Contains(TEXT("MobaMenu"), ESearchCase::IgnoreCase);
		if (bMenuMap)
		{
			if (GI->ShouldShowLobby())
			{
				GI->ShowLobby();
			}
			else
			{
				GI->ShowMenu();
			}
		}
		GI->HideLoadingScreen();
	}
}
