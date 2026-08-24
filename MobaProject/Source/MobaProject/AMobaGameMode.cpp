#include "AMobaGameMode.h"
#include "AMobaPlayerState.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "MobaBaseCharacter.h"
#include "MobaGameInstance.h"
#include "MobaPlayerStart.h"

AAMobaGameMode::AAMobaGameMode()
{
	DefaultPawnClass = AMobaBaseCharacter::StaticClass();
	PlayerStateClass = AMobaPlayerState::StaticClass();
	bUseSeamlessTravel = false;
}

void AAMobaGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	bUseSeamlessTravel = false;
	ExpectedPlayers = FMath::Max(1, UGameplayStatics::GetIntOption(Options, TEXT("WaitPlayers"), 1));
	bMatchUnlocked = false;
}

void AAMobaGameMode::DestroyOrphanHeroes()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AMobaBaseCharacter*> Orphans;
	for (TActorIterator<AMobaBaseCharacter> It(World); It; ++It)
	{
		AMobaBaseCharacter* Hero = *It;
		if (!Hero || !IsValid(Hero) || Hero->IsPendingKillPending())
		{
			continue;
		}
		// Placed leftovers and failed RestartPlayer extras have no player.
		if (!Cast<APlayerController>(Hero->GetController()))
		{
			Orphans.Add(Hero);
		}
	}
	for (AMobaBaseCharacter* Hero : Orphans)
	{
		Hero->Destroy();
	}
}

void AAMobaGameMode::StartPlay()
{
	DestroyOrphanHeroes();
	Super::StartPlay();
	DestroyOrphanHeroes();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			WaitPlayersTimer,
			this,
			&AAMobaGameMode::OnWaitForPlayersTimeout,
			20.f,
			false);
	}
	TryUnlockMatch(false);
}

void AAMobaGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (AMobaPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AMobaPlayerState>() : nullptr)
	{
		PS->AssignLobbyName();
		if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
		{
			const int32 CachedHero = GI->TakeCachedLobbyHero(PS);
			if (CachedHero == 0 || CachedHero == 1)
			{
				PS->HeroIndex = CachedHero;
			}
		}
	}
	AssignTeam(NewPlayer);
	DestroyOrphanHeroes();

	if (NewPlayer)
	{
		if (APawn* Existing = NewPlayer->GetPawn())
		{
			UClass* Wanted = GetDefaultPawnClassForController(NewPlayer);
			const bool bKeep = Wanted && Existing->IsA(Wanted);
			if (!bKeep)
			{
				NewPlayer->UnPossess();
				Existing->Destroy();
			}
		}
	}
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	DestroyOrphanHeroes();

	if (AMobaBaseCharacter* Hero = NewPlayer ? Cast<AMobaBaseCharacter>(NewPlayer->GetPawn()) : nullptr)
	{
		const AMobaPlayerState* PS = NewPlayer->GetPlayerState<AMobaPlayerState>();
		Hero->SetTeamId((PS && (PS->TeamID == 1 || PS->TeamID == 2)) ? PS->TeamID : 1);
	}
	TryUnlockMatch(false);
}

void AAMobaGameMode::NotifyPlayerLoaded()
{
	TryUnlockMatch(false);
}

void AAMobaGameMode::OnWaitForPlayersTimeout()
{
	TryUnlockMatch(true);
}

void AAMobaGameMode::TryUnlockMatch(bool bForce)
{
	if (bMatchUnlocked)
	{
		return;
	}
	UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}

	int32 Loaded = 0;
	int32 Present = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		AMobaPlayerState* MobaPS = Cast<AMobaPlayerState>(PS);
		if (!MobaPS || MobaPS->IsOnlyASpectator())
		{
			continue;
		}
		++Present;
		if (MobaPS->HasLoadedMap())
		{
			++Loaded;
		}
	}

	const int32 Need = FMath::Max(ExpectedPlayers, 1);
	if (!bForce && (Present < Need || Loaded < Need))
	{
		return;
	}
	UnlockMatch();
}

void AAMobaGameMode::UnlockMatch()
{
	if (bMatchUnlocked)
	{
		return;
	}
	bMatchUnlocked = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaitPlayersTimer);
		if (AGameStateBase* GS = World->GetGameState())
		{
			for (APlayerState* PS : GS->PlayerArray)
			{
				if (AMobaPlayerState* MobaPS = Cast<AMobaPlayerState>(PS))
				{
					MobaPS->bMatchUnlocked = true;
				}
			}
		}
	}
	if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
	{
		GI->OnMatchUnlocked();
	}
}

UClass* AAMobaGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (const AMobaPlayerState* PS = InController ? InController->GetPlayerState<AMobaPlayerState>() : nullptr)
	{
		if (const UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
		{
			if (UClass* Hero = GI->GetHeroClassAt(PS->HeroIndex))
			{
				return Hero;
			}
		}
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void AAMobaGameMode::RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot)
{
	AssignTeam(NewPlayer);

	AMobaPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AMobaPlayerState>() : nullptr;
	const int32 Team = (PS && (PS->TeamID == 1 || PS->TeamID == 2)) ? PS->TeamID : 1;
	if (PS)
	{
		PS->TeamID = Team;
	}

	Super::RestartPlayerAtPlayerStart(NewPlayer, StartSpot);
	DestroyOrphanHeroes();

	if (AMobaBaseCharacter* Hero = NewPlayer ? Cast<AMobaBaseCharacter>(NewPlayer->GetPawn()) : nullptr)
	{
		Hero->SetTeamId(Team);
		if (StartSpot)
		{
			Hero->SnapFacingToSpawn(StartSpot->GetActorRotation());
		}
		Hero->ScheduleShopRangeRefresh();
	}
}

void AAMobaGameMode::AssignTeam(AController* Player)
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
	if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
	{
		const int32 Cached = GI->TakeCachedLobbyTeam(PS);
		if (Cached == 1 || Cached == 2)
		{
			PS->TeamID = Cached;
			return;
		}
	}
	PS->TeamID = NextTeamId();
}

int32 AAMobaGameMode::NextTeamId()
{
	const int32 Team = (NextJoinTeam == 2) ? 2 : 1;
	NextJoinTeam = (Team == 1) ? 2 : 1;
	return Team;
}

int32 AAMobaGameMode::GetStartTeamId(const AActor* Start) const
{
	if (const AMobaPlayerStart* MobaStart = Cast<AMobaPlayerStart>(Start))
	{
		return (MobaStart->TeamID == 1 || MobaStart->TeamID == 2) ? MobaStart->TeamID : 0;
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
	AssignTeam(Player);
	int32 Team = 1;
	if (const AMobaPlayerState* PS = Player ? Player->GetPlayerState<AMobaPlayerState>() : nullptr)
	{
		Team = (PS->TeamID == 2) ? 2 : 1;
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
