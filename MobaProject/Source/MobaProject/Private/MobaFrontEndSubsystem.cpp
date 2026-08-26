#include "MobaFrontEndSubsystem.h"
#include "Camera/CameraActor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "MobaGameInstance.h"
#include "MobaLoadingWidget.h"
#include "MobaLobbyWidget.h"
#include "MobaMenuWidget.h"
#include "MobaSessionSubsystem.h"
#include "MobaSettingsWidget.h"
#if !UE_SERVER
#include "MoviePlayer.h"
#endif
#include "TimerManager.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

void UMobaFrontEndSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency(UMobaSessionSubsystem::StaticClass());
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &UMobaFrontEndSubsystem::HandlePreLoadMap);
	if (!IsRunningDedicatedServer())
	{
		SetupMovieLoadingScreen(TEXT("LOADING..."));
	}
}

void UMobaFrontEndSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	HideLoadingScreen();
	Super::Deinitialize();
}

void UMobaFrontEndSubsystem::ApplyUiPointer(UUserWidget* FocusWidget)
{
	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}

	UGameViewportClient* Viewport = nullptr;
	if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
	{
		Viewport = LocalPlayer->ViewportClient;
	}
	if (!Viewport && PC->GetWorld())
	{
		Viewport = PC->GetWorld()->GetGameViewport();
	}
	if (Viewport)
	{
		Viewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
		Viewport->SetMouseLockMode(EMouseLockMode::DoNotLock);
		Viewport->SetHideCursorDuringCapture(false);
	}

	PC->bShowMouseCursor = true;
	PC->DefaultMouseCursor = EMouseCursor::Default;
	PC->CurrentMouseCursor = EMouseCursor::Default;
	PC->SetIgnoreLookInput(true);
	PC->SetIgnoreMoveInput(true);

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (FocusWidget)
	{
		InputMode.SetWidgetToFocus(FocusWidget->TakeWidget());
	}
	PC->SetInputMode(InputMode);
	PC->FlushPressedKeys();
}

bool UMobaFrontEndSubsystem::IsMenuUiActive() const
{
	auto InView = [](const UUserWidget* Widget)
	{
		return IsValid(Widget) && Widget->IsInViewport();
	};
	return InView(MenuWidget) || InView(LobbyWidget) || InView(SettingsWidget) || InView(LoadingWidget);
}

void UMobaFrontEndSubsystem::RestoreUiPointerIfNeeded()
{
	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!PC || !IsMenuUiActive())
	{
		return;
	}

	bool bNeedRestore = !PC->bShowMouseCursor;
	if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
	{
		if (UGameViewportClient* Viewport = LocalPlayer->ViewportClient)
		{
			const EMouseCaptureMode Capture = Viewport->GetMouseCaptureMode();
			if (Capture == EMouseCaptureMode::CapturePermanently
				|| Capture == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown)
			{
				bNeedRestore = true;
			}
		}
	}
	if (!bNeedRestore)
	{
		PC->bShowMouseCursor = true;
		return;
	}

	UUserWidget* Focus = nullptr;
	if (IsValid(SettingsWidget) && SettingsWidget->IsInViewport())
	{
		Focus = SettingsWidget;
	}
	else if (IsValid(LoadingWidget) && LoadingWidget->IsInViewport())
	{
		Focus = LoadingWidget;
	}
	else if (IsValid(LobbyWidget) && LobbyWidget->IsInViewport())
	{
		Focus = LobbyWidget;
	}
	else if (IsValid(MenuWidget) && MenuWidget->IsInViewport())
	{
		Focus = MenuWidget;
	}
	ApplyUiPointer(Focus);
}

void UMobaFrontEndSubsystem::ApplySettingsInput()
{
	if (IsValid(SettingsWidget))
	{
		ApplyUiPointer(SettingsWidget);
	}
}

void UMobaFrontEndSubsystem::ShowSettings()
{
	if (IsShowingLoading())
	{
		return;
	}
	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	if (IsValid(SettingsWidget))
	{
		SettingsWidget->RemoveFromParent();
	}
	SettingsWidget = CreateWidget<UMobaSettingsWidget>(PC, UMobaSettingsWidget::StaticClass());
	if (!SettingsWidget)
	{
		return;
	}
	SettingsWidget->PlaceInViewport();
	ApplySettingsInput();
}

void UMobaFrontEndSubsystem::HideSettings()
{
	if (IsValid(SettingsWidget))
	{
		SettingsWidget->RemoveFromParent();
	}
	SettingsWidget = nullptr;

	UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance());
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	UMobaSessionSubsystem* Session = GI ? GI->GetSubsystem<UMobaSessionSubsystem>() : nullptr;
	if (World && GI && Session)
	{
		const FString MapName = World->GetMapName();
		if (MapName.Contains(TEXT("MobaMenu"), ESearchCase::IgnoreCase))
		{
			if (Session->ShouldShowLobby())
			{
				ShowLobby();
			}
			else
			{
				ShowMenu();
			}
			return;
		}
	}
	ReleaseMenuInput();
}

void UMobaFrontEndSubsystem::ToggleSettings()
{
	if (IsShowingLoading())
	{
		return;
	}
	if (IsValid(SettingsWidget) && SettingsWidget->IsInViewport())
	{
		HideSettings();
		return;
	}
	ShowSettings();
}

void UMobaFrontEndSubsystem::ReleaseMenuInput()
{
	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}

	PC->SetIgnoreLookInput(false);
	PC->SetIgnoreMoveInput(false);
	if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
	{
		if (UGameViewportClient* Viewport = LocalPlayer->ViewportClient)
		{
			Viewport->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);
			Viewport->SetHideCursorDuringCapture(true);
		}
	}
	PC->SetInputMode(FInputModeGameOnly());
	PC->bShowMouseCursor = false;
	PC->FlushPressedKeys();
}

void UMobaFrontEndSubsystem::HideMenu()
{
	if (IsValid(SettingsWidget))
	{
		SettingsWidget->RemoveFromParent();
	}
	SettingsWidget = nullptr;
	if (IsValid(MenuWidget))
	{
		MenuWidget->RemoveFromParent();
	}
	MenuWidget = nullptr;
	HideLobby();
}

void UMobaFrontEndSubsystem::HideLobby()
{
	if (IsValid(LobbyWidget))
	{
		LobbyWidget->RemoveFromParent();
	}
	LobbyWidget = nullptr;
}

void UMobaFrontEndSubsystem::ApplyLobbyInput()
{
	if (IsValid(LobbyWidget))
	{
		ApplyUiPointer(LobbyWidget);
	}
}

void UMobaFrontEndSubsystem::ApplyMenuCamera()
{
	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	UWorld* World = PC ? PC->GetWorld() : GetWorld();
	if (!PC || !PC->IsLocalController() || !World)
	{
		return;
	}
	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		PC->SetViewTarget(*It);
		break;
	}
}

void UMobaFrontEndSubsystem::ShowLobby()
{
	HideLoadingScreen();
	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!PC)
	{
		if (UWorld* World = GI ? GI->GetWorld() : nullptr)
		{
			World->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateUObject(this, &UMobaFrontEndSubsystem::ShowLobby));
		}
		return;
	}
	ApplyMenuCamera();

	if (IsValid(MenuWidget))
	{
		MenuWidget->RemoveFromParent();
	}
	MenuWidget = nullptr;
	if (IsValid(SettingsWidget))
	{
		SettingsWidget->RemoveFromParent();
	}
	SettingsWidget = nullptr;
	if (IsValid(LobbyWidget))
	{
		LobbyWidget->RemoveFromParent();
	}
	LobbyWidget = CreateWidget<UMobaLobbyWidget>(PC, UMobaLobbyWidget::StaticClass());
	if (!LobbyWidget)
	{
		return;
	}
	LobbyWidget->PlaceInViewport();
	ApplyLobbyInput();
	if (UMobaSessionSubsystem* Session = GI->GetSubsystem<UMobaSessionSubsystem>())
	{
		Session->ApplyLocalHeroChoice();
	}
}

void UMobaFrontEndSubsystem::ShowLoadingScreen(const FString& Message, bool bPrepareMovie, bool bCaptureInput)
{
	const FString Text = Message.IsEmpty() ? TEXT("LOADING...") : Message;
	bLoadingScreenQueued = true;

	if (IsValid(SettingsWidget))
	{
		SettingsWidget->RemoveFromParent();
	}
	SettingsWidget = nullptr;
	if (IsValid(MenuWidget))
	{
		MenuWidget->RemoveFromParent();
	}
	MenuWidget = nullptr;
	HideLobby();

	if (IsValid(LoadingWidget))
	{
		LoadingWidget->RemoveFromParent();
	}
	LoadingWidget = CreateWidget<UMobaLoadingWidget>(GetGameInstance(), UMobaLoadingWidget::StaticClass());
	if (LoadingWidget)
	{
		LoadingWidget->SetMessage(Text);
		LoadingWidget->PlaceInViewport();
	}
	if (bPrepareMovie)
	{
		SetupMovieLoadingScreen(Text);
	}
	if (bCaptureInput)
	{
		ApplyUiPointer(LoadingWidget);
	}
}

bool UMobaFrontEndSubsystem::IsShowingLoading() const
{
	return bLoadingScreenQueued || (IsValid(LoadingWidget) && LoadingWidget->IsInViewport());
}

void UMobaFrontEndSubsystem::HideLoadingScreen()
{
	bLoadingScreenQueued = false;
	StopLoadingMovie();
	if (IsValid(LoadingWidget))
	{
		LoadingWidget->RemoveFromParent();
	}
	LoadingWidget = nullptr;

	UMobaSessionSubsystem* Session = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UMobaSessionSubsystem>()
		: nullptr;
	if (Session && Session->ShouldShowLobby())
	{
		RestoreUiPointerIfNeeded();
		return;
	}
	UWorld* World = GetWorld();
	if (World && World->GetMapName().Contains(TEXT("MobaMenu"), ESearchCase::IgnoreCase))
	{
		RestoreUiPointerIfNeeded();
		return;
	}
	ReleaseMenuInput();
}

void UMobaFrontEndSubsystem::HandlePreLoadMap(const FWorldContext& LoadedContext, const FString& MapName)
{
	UGameInstance* GI = GetGameInstance();
	const FWorldContext* Mine = GI ? GI->GetWorldContext() : nullptr;
	if (!Mine || Mine != &LoadedContext)
	{
		return;
	}
	(void)MapName;
	ShowLoadingScreen(TEXT("LOADING..."));
}

void UMobaFrontEndSubsystem::StopLoadingMovie()
{
#if !UE_SERVER
	if (GetMoviePlayer() && GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		GetMoviePlayer()->StopMovie();
	}
#endif
}

void UMobaFrontEndSubsystem::SetupMovieLoadingScreen(const FString& Message)
{
#if !UE_SERVER
	if (IsRunningDedicatedServer())
	{
		return;
	}
	IGameMoviePlayer* MoviePlayer = GetMoviePlayer();
	if (!MoviePlayer)
	{
		return;
	}
	const FString Text = Message.IsEmpty() ? TEXT("LOADING...") : Message;
	FLoadingScreenAttributes Attr;
	Attr.bAutoCompleteWhenLoadingCompletes = true;
	Attr.bMoviesAreSkippable = true;
	Attr.bWaitForManualStop = false;
	Attr.MinimumLoadingScreenDisplayTime = 1.0f;
	Attr.WidgetLoadingScreen =
		SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.004f, 0.039f, 0.075f, 1.f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Text))
				.Justification(ETextJustify::Center)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.784f, 0.667f, 0.431f, 1.f)))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 36))
			]
		];
	MoviePlayer->SetupLoadingScreen(Attr);
#else
	(void)Message;
#endif
}

void UMobaFrontEndSubsystem::ShowJoinLoadout()
{
	if (UMobaSessionSubsystem* Session = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMobaSessionSubsystem>() : nullptr)
	{
		Session->SetJoinLoadout(true);
	}
	HideLoadingScreen();
	ShowLobby();
	if (IsValid(LobbyWidget))
	{
		LobbyWidget->SetJoinInProgress(true);
	}
}

void UMobaFrontEndSubsystem::FinishJoinLoadout()
{
	if (UMobaSessionSubsystem* Session = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMobaSessionSubsystem>() : nullptr)
	{
		Session->SetJoinLoadout(false);
	}
	HideLobby();
	HideLoadingScreen();
	ReleaseMenuInput();
}

void UMobaFrontEndSubsystem::ShowMenu()
{
	HideLoadingScreen();
	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!PC)
	{
		if (UWorld* World = GI ? GI->GetWorld() : nullptr)
		{
			World->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateUObject(this, &UMobaFrontEndSubsystem::ShowMenu));
		}
		return;
	}

	HideLobby();
	ApplyMenuCamera();
	if (IsValid(MenuWidget))
	{
		MenuWidget->RemoveFromParent();
	}
	MenuWidget = CreateWidget<UMobaMenuWidget>(PC, UMobaMenuWidget::StaticClass());
	if (MenuWidget)
	{
		MenuWidget->AddToViewport(100);
	}
	if (!MenuWidget)
	{
		return;
	}

	FString Notice;
	if (UMobaSessionSubsystem* Session = GI->GetSubsystem<UMobaSessionSubsystem>())
	{
		Session->ConsumeJoinError(Notice);
	}
	MenuWidget->SetNotice(Notice);
	ApplyUiPointer(MenuWidget);
}
