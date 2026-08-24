#include "MobaVictoryManager.h"
#include "AMobaPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "MobaBaseCharacter.h"
#include "MobaEndHUD.h"
#include "MobaTower.h"
#include "Net/UnrealNetwork.h"

AMobaVictoryManager::AMobaVictoryManager()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AMobaVictoryManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMobaVictoryManager, bMatchOver);
	DOREPLIFETIME(AMobaVictoryManager, WinningTeam);
}

void AMobaVictoryManager::BeginPlay()
{
	Super::BeginPlay();
	if (bMatchOver)
	{
		ShowLocalEndScreen();
	}
}

void AMobaVictoryManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && !bMatchOver)
	{
		CheckTowers();
	}
}

void AMobaVictoryManager::CheckTowers()
{
	if (Team1Tower && (Team1Tower->IsDead() || Team1Tower->GetHealth() <= 0.f))
	{
		NotifyTowerDestroyed(Team1Tower);
		return;
	}
	if (Team2Tower && (Team2Tower->IsDead() || Team2Tower->GetHealth() <= 0.f))
	{
		NotifyTowerDestroyed(Team2Tower);
	}
}

void AMobaVictoryManager::NotifyTowerDestroyed(AMobaTower* Tower)
{
	if (!HasAuthority() || bMatchOver || !Tower)
	{
		return;
	}

	if (Tower == Team1Tower)
	{
		EndMatch(1);
	}
	else if (Tower == Team2Tower)
	{
		EndMatch(2);
	}
}

void AMobaVictoryManager::EndMatch(int32 DestroyedTowerTeam)
{
	if (bMatchOver)
	{
		return;
	}

	WinningTeam = (DestroyedTowerTeam == 1) ? 2 : 1;
	bMatchOver = true;
	MulticastMatchOver(WinningTeam);
}

void AMobaVictoryManager::MulticastMatchOver_Implementation(int32 InWinningTeam)
{
	WinningTeam = InWinningTeam;
	bMatchOver = true;
	ShowLocalEndScreen();
}

void AMobaVictoryManager::OnRep_MatchOver()
{
	if (bMatchOver)
	{
		ShowLocalEndScreen();
	}
}

void AMobaVictoryManager::NotifyLoading(bool bPlayAgain)
{
	if (HasAuthority())
	{
		MulticastLoading(bPlayAgain);
		return;
	}
	MulticastLoading_Implementation(bPlayAgain);
}

void AMobaVictoryManager::MulticastLoading_Implementation(bool bPlayAgain)
{
	if (!EndHUD)
	{
		ShowLocalEndScreen();
	}
	if (EndHUD)
	{
		EndHUD->ShowLoading(bPlayAgain ? TEXT("LOADING...") : TEXT("RETURNING TO MENU..."));
	}
}

void AMobaVictoryManager::ShowLocalEndScreen()
{
	if (EndHUD || !GetWorld())
	{
		return;
	}

	APlayerController* LocalPC = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController())
		{
			LocalPC = PC;
			break;
		}
	}
	if (!LocalPC)
	{
		return;
	}

	int32 LocalTeam = 0;
	if (const AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(LocalPC->GetPawn()))
	{
		LocalTeam = Hero->GetTeamId();
	}
	else if (const AMobaPlayerState* PS = LocalPC->GetPlayerState<AMobaPlayerState>())
	{
		LocalTeam = PS->TeamID;
	}

	EndHUD = CreateWidget<UMobaEndHUD>(LocalPC, UMobaEndHUD::StaticClass());
	if (!EndHUD)
	{
		return;
	}

	EndHUD->ShowResult(LocalTeam != 0 && LocalTeam == WinningTeam);
	EndHUD->PlaceInViewport();

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(EndHUD->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	LocalPC->SetInputMode(InputMode);
	LocalPC->bShowMouseCursor = true;
	LocalPC->FlushPressedKeys();
}
