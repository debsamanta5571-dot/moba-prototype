#include "MobaMenuGameMode.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "MobaMenuWidget.h"

AMobaMenuGameMode::AMobaMenuGameMode()
{
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}

void AMobaMenuGameMode::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	MenuWidget = CreateWidget<UMobaMenuWidget>(PC, UMobaMenuWidget::StaticClass());
	if (!MenuWidget)
	{
		return;
	}

	MenuWidget->AddToViewport(100);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;
}
