#include "MobaGameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

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

void UMobaGameInstance::HostGame()
{
	ReleaseMenuInput();
	UGameplayStatics::OpenLevel(this, ArenaMap, true, TEXT("listen"));
}

void UMobaGameInstance::JoinGame(const FString& Address)
{
	FString Url = Address.TrimStartAndEnd();
	if (Url.IsEmpty())
	{
		Url = TEXT("127.0.0.1");
	}

	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC)
	{
		return;
	}

	ReleaseMenuInput();
	PC->ClientTravel(Url, TRAVEL_Absolute);
}

void UMobaGameInstance::LoadComplete(const float LoadTime, const FString& MapName)
{
	Super::LoadComplete(LoadTime, MapName);

	if (!MapName.Contains(TEXT("MobaMenu")))
	{
		ReleaseMenuInput();
	}
}
