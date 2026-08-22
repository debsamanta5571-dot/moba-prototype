#include "MobaRespawnHUD.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "MobaBaseCharacter.h"
#include "Styling/CoreStyle.h"

UMobaRespawnHUD::UMobaRespawnHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasScriptImplementedTick = true;
}

void UMobaRespawnHUD::SetOwnerCharacter(AMobaBaseCharacter* InOwner)
{
	OwnerCharacter = InOwner;
	UpdateTimer();
}

void UMobaRespawnHUD::PlaceInViewport()
{
	SetIsFocusable(false);

	FGameViewportWidgetSlot ViewportSlot;
	ViewportSlot.Anchors = FAnchors(0.5f, 0.28f, 0.5f, 0.28f);
	ViewportSlot.Alignment = FVector2D(0.5f, 0.5f);
	ViewportSlot.Offsets = FMargin(0.f, 0.f, 480.f, 90.f);
	ViewportSlot.ZOrder = 70;

	UGameViewportSubsystem* Viewport = UGameViewportSubsystem::Get();
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (Viewport && LocalPlayer)
	{
		if (Viewport->IsWidgetAdded(this))
		{
			Viewport->SetWidgetSlot(this, ViewportSlot);
		}
		else
		{
			Viewport->AddWidgetForPlayer(this, LocalPlayer, ViewportSlot);
		}
		return;
	}

	if (!IsInViewport())
	{
		AddToPlayerScreen(70);
	}
	SetAnchorsInViewport(FAnchors(0.5f, 0.28f, 0.5f, 0.28f));
	SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
}

TSharedRef<SWidget> UMobaRespawnHUD::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		Rebuild();
	}
	return Super::RebuildWidget();
}

void UMobaRespawnHUD::Rebuild()
{
	TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TimerText"));
	TimerText->SetJustification(ETextJustify::Center);
	TimerText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.82f, 0.28f, 1.f)));
	TimerText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 36));
	TimerText->SetText(FText::FromString(TEXT("RESPAWN  5")));
	WidgetTree->RootWidget = TimerText;
}

void UMobaRespawnHUD::NativeConstruct()
{
	Super::NativeConstruct();
	if (!OwnerCharacter)
	{
		OwnerCharacter = Cast<AMobaBaseCharacter>(GetOwningPlayerPawn());
	}
	PlaceInViewport();
	UpdateTimer();
}

void UMobaRespawnHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateTimer();
}

void UMobaRespawnHUD::UpdateTimer()
{
	if (!TimerText)
	{
		return;
	}

	const float Remaining = OwnerCharacter ? OwnerCharacter->GetRespawnRemaining() : 0.f;
	if (Remaining <= 0.f)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TimerText->SetText(FText::FromString(FString::Printf(
		TEXT("RESPAWN  %d"),
		FMath::Max(1, FMath::CeilToInt(Remaining)))));
}
