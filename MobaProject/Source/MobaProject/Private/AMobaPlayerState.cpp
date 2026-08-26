#include "AMobaPlayerState.h"
#include "MobaGameMode.h"
#include "Containers/Set.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "MobaGameInstance.h"
#include "Net/UnrealNetwork.h"

AMobaPlayerState::AMobaPlayerState()
{
}

void AMobaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMobaPlayerState, TeamID);
	DOREPLIFETIME(AMobaPlayerState, HeroIndex);
	DOREPLIFETIME(AMobaPlayerState, bMatchUnlocked);
	DOREPLIFETIME(AMobaPlayerState, bAwaitingLoadout);
	DOREPLIFETIME(AMobaPlayerState, bLobbyLeader);
}

void AMobaPlayerState::ClientShowLoading_Implementation(const FString& Message)
{
	if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
	{
		GI->ShowLoadingScreen(Message.IsEmpty() ? TEXT("LOADING...") : Message);
	}
}

bool AMobaPlayerState::CanEditLoadout() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	if (World->GetMapName().Contains(TEXT("MobaMenu")))
	{
		return true;
	}
	return bAwaitingLoadout;
}

void AMobaPlayerState::SetAwaitingLoadout(bool bAwaiting)
{
	bAwaitingLoadout = bAwaiting;
}

void AMobaPlayerState::ServerSetTeam_Implementation(int32 NewTeam)
{
	if (NewTeam != 1 && NewTeam != 2)
	{
		return;
	}
	if (!CanEditLoadout())
	{
		return;
	}
	TeamID = NewTeam;
}

void AMobaPlayerState::ServerSetHeroIndex_Implementation(int32 NewIndex)
{
	if (NewIndex != 0 && NewIndex != 1)
	{
		return;
	}
	const AController* OwnerController = GetOwningController();
	const bool bHasPawn = OwnerController && OwnerController->GetPawn();
	if (bHasPawn && !CanEditLoadout())
	{
		return;
	}
	HeroIndex = NewIndex;
}

void AMobaPlayerState::ServerConfirmLoadout_Implementation(int32 NewHero, int32 NewTeam)
{
	if (!HasAuthority())
	{
		return;
	}
	AController* OwnerController = GetOwningController();
	if (OwnerController && OwnerController->GetPawn())
	{
		bAwaitingLoadout = false;
		return;
	}
	AMobaGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMobaGameMode>() : nullptr;
	const bool bLate = bAwaitingLoadout || (GM && GM->IsMatchUnlocked());
	if (!bLate)
	{
		return;
	}
	if (NewHero == 0 || NewHero == 1)
	{
		HeroIndex = NewHero;
	}
	if (NewTeam == 1 || NewTeam == 2)
	{
		TeamID = NewTeam;
	}
	bAwaitingLoadout = false;
	if (GM)
	{
		GM->SpawnLateJoiner(OwnerController);
	}
}

void AMobaPlayerState::ServerStartMatchFromLobby_Implementation()
{
	if (!HasAuthority() || !bLobbyLeader)
	{
		return; // second joiner's Start Game is hidden, but don't trust the client anyway
	}
	if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
	{
		GI->AuthorityStartMatchFromLobby();
	}
}

void AMobaPlayerState::ServerNotifyMapLoaded_Implementation()
{
	MarkMapLoaded();
}

void AMobaPlayerState::MarkMapLoaded()
{
	if (!HasAuthority() || bMapLoaded)
	{
		return;
	}
	bMapLoaded = true;
	if (UWorld* World = GetWorld())
	{
		if (AMobaGameMode* GM = World->GetAuthGameMode<AMobaGameMode>())
		{
			GM->NotifyPlayerLoaded();
		}
	}
}

void AMobaPlayerState::OnRep_MatchUnlocked()
{
	if (!bMatchUnlocked)
	{
		return;
	}
	if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
	{
		GI->OnMatchUnlocked();
	}
}

void AMobaPlayerState::AssignLobbyName()
{
	if (!HasAuthority())
	{
		return;
	}

	const FString Current = GetPlayerName();
	if (Current.StartsWith(TEXT("Player ")) && Current.Len() > 7 && FCString::Atoi(*Current.RightChop(7)) > 0)
	{
		return;
	}

	TSet<int32> Used;
	if (UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GS = World->GetGameState())
		{
			for (APlayerState* Other : GS->PlayerArray)
			{
				if (!Other || Other == this)
				{
					continue;
				}
				const FString Name = Other->GetPlayerName();
				if (Name.StartsWith(TEXT("Player ")))
				{
					const int32 Number = FCString::Atoi(*Name.RightChop(7));
					if (Number > 0)
					{
						Used.Add(Number);
					}
				}
			}
		}
	}

	int32 Number = 1;
	while (Used.Contains(Number))
	{
		++Number;
	}
	SetPlayerName(FString::Printf(TEXT("Player %d"), Number));
}

void AMobaPlayerState::OnRep_TeamId()
{
}

void AMobaPlayerState::OnRep_AwaitingLoadout()
{
	APlayerController* PC = Cast<APlayerController>(GetOwningController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
	{
		if (bAwaitingLoadout)
		{
			GI->ShowJoinLoadout();
		}
		else
		{
			GI->FinishJoinLoadout();
		}
	}
}
