#include "AMobaPlayerState.h"
#include "../AMobaGameMode.h"
#include "Containers/Set.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
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
}

void AMobaPlayerState::ClientShowLoading_Implementation(const FString& Message)
{
	if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
	{
		GI->ShowLoadingScreen(Message.IsEmpty() ? TEXT("LOADING...") : Message);
	}
}

void AMobaPlayerState::ServerSetTeam_Implementation(int32 NewTeam)
{
	if (NewTeam != 1 && NewTeam != 2)
	{
		return;
	}
	const UWorld* World = GetWorld();
	if (World && !World->GetMapName().Contains(TEXT("MobaMenu")))
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
	const UWorld* World = GetWorld();
	if (World && !World->GetMapName().Contains(TEXT("MobaMenu")))
	{
		return;
	}
	HeroIndex = NewIndex;
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
		if (AAMobaGameMode* GM = World->GetAuthGameMode<AAMobaGameMode>())
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
