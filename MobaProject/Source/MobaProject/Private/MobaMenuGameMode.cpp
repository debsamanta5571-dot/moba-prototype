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
	if (AMobaPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AMobaPlayerState>() : nullptr)
	{
		if (!PS->bLobbyLeader)
		{
			AGameStateBase* GS = GameState.Get();
			if (!GS && GetWorld())
			{
				GS = GetWorld()->GetGameState();
			}
			bool bAnyLeader = false;
			if (GS)
			{
				for (APlayerState* Other : GS->PlayerArray)
				{
					if (const AMobaPlayerState* OtherMoba = Cast<AMobaPlayerState>(Other))
					{
						if (OtherMoba->bLobbyLeader)
						{
							bAnyLeader = true;
							break;
						}
					}
				}
			}
			if (!bAnyLeader)
			{
				PS->bLobbyLeader = true;
			}
		}
	}
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

// Dedicated server has no host pawn. First real player gets Start Game; next in list if they leave.
void AMobaMenuGameMode::EnsureLobbyLeader()
{
	AGameStateBase* GS = GameState;
	if (!GS)
	{
		GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	}
	if (!GS)
	{
		return;
	}

	AMobaPlayerState* First = nullptr;
	bool bHasLeader = false;
	for (APlayerState* PS : GS->PlayerArray)
	{
		AMobaPlayerState* MobaPS = Cast<AMobaPlayerState>(PS);
		if (!MobaPS || MobaPS->IsOnlyASpectator())
		{
			continue;
		}
		if (!First)
		{
			First = MobaPS;
		}
		if (MobaPS->bLobbyLeader)
		{
			bHasLeader = true;
		}
	}
	if (!bHasLeader && First)
	{
		First->bLobbyLeader = true;
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
