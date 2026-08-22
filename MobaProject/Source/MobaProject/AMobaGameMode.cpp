#include "AMobaGameMode.h"
#include "AMobaPlayerState.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "MobaBaseCharacter.h"
#include "MobaPlayerStart.h"

AAMobaGameMode::AAMobaGameMode()
{
	DefaultPawnClass = AMobaBaseCharacter::StaticClass();
	PlayerStateClass = AMobaPlayerState::StaticClass();
}

void AAMobaGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void AAMobaGameMode::RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot)
{
	const int32 StartTeam = GetStartTeamId(StartSpot);
	int32 Team = StartTeam;
	if (Team == 0)
	{
		if (const AMobaPlayerState* Existing = NewPlayer ? NewPlayer->GetPlayerState<AMobaPlayerState>() : nullptr)
		{
			Team = Existing->TeamID;
		}
	}
	if (Team == 0)
	{
		Team = CountTeam(1) <= CountTeam(2) ? 1 : 2;
	}

	if (AMobaPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AMobaPlayerState>() : nullptr)
	{
		PS->TeamID = Team;
	}

	Super::RestartPlayerAtPlayerStart(NewPlayer, StartSpot);

	if (AMobaBaseCharacter* Hero = NewPlayer ? Cast<AMobaBaseCharacter>(NewPlayer->GetPawn()) : nullptr)
	{
		Hero->SetTeamId(Team);
	}
}

void AAMobaGameMode::AssignTeam(APlayerController* Player)
{
	if (!Player)
	{
		return;
	}

	AMobaPlayerState* PS = Player->GetPlayerState<AMobaPlayerState>();
	if (!PS || PS->TeamID != 0)
	{
		return;
	}

	PS->TeamID = CountTeam(1) <= CountTeam(2) ? 1 : 2;
}

int32 AAMobaGameMode::CountTeam(int32 TeamId) const
{
	int32 Count = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const AMobaPlayerState* PS = PC ? PC->GetPlayerState<AMobaPlayerState>() : nullptr;
		if (PS && PS->TeamID == TeamId)
		{
			++Count;
		}
	}
	return Count;
}

int32 AAMobaGameMode::GetStartTeamId(const AActor* Start) const
{
	if (const AMobaPlayerStart* MobaStart = Cast<AMobaPlayerStart>(Start))
	{
		return MobaStart->TeamID;
	}
	if (const APlayerStart* PlayerStart = Cast<APlayerStart>(Start))
	{
		const FString Tag = PlayerStart->PlayerStartTag.ToString();
		if (Tag == TEXT("1") || Tag.Equals(TEXT("Team1"), ESearchCase::IgnoreCase))
		{
			return 1;
		}
		if (Tag == TEXT("2") || Tag.Equals(TEXT("Team2"), ESearchCase::IgnoreCase))
		{
			return 2;
		}
	}
	return 0;
}

AActor* AAMobaGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	int32 Team = 0;
	if (const AMobaPlayerState* PS = Player ? Player->GetPlayerState<AMobaPlayerState>() : nullptr)
	{
		Team = PS->TeamID;
	}
	if (Team == 0)
	{
		Team = CountTeam(1) <= CountTeam(2) ? 1 : 2;
	}

	TArray<AActor*> TeamStarts;
	AActor* Untagged = nullptr;

	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* Start = *It;
		if (!Start)
		{
			continue;
		}

		const int32 StartTeam = GetStartTeamId(Start);
		if (Team != 0 && StartTeam == Team)
		{
			TeamStarts.Add(Start);
		}
		else if (StartTeam == 0 && !Untagged)
		{
			Untagged = Start;
		}
	}

	if (TeamStarts.Num() > 0)
	{
		return TeamStarts[FMath::RandRange(0, TeamStarts.Num() - 1)];
	}
	if (Untagged)
	{
		return Untagged;
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}
