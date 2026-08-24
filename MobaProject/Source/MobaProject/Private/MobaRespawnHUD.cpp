#include "MobaRespawnHUD.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
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
	ViewportSlot.Anchors = FAnchors(0.f, 0.f, 1.f, 1.f);
	ViewportSlot.Alignment = FVector2D(0.f, 0.f);
	ViewportSlot.Offsets = FMargin(0.f, 0.f, 0.f, 0.f);
	ViewportSlot.ZOrder = 90;

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
	}
	else if (!IsInViewport())
	{
		AddToPlayerScreen(90);
		SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
		SetAlignmentInViewport(FVector2D(0.f, 0.f));
	}

	Refresh();
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
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.55f));
	Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim))
	{
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);
	}

	USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CardSize"));
	CardSize->SetWidthOverride(420.f);
	if (UOverlaySlot* Center = Root->AddChildToOverlay(CardSize))
	{
		Center->SetHorizontalAlignment(HAlign_Center);
		Center->SetVerticalAlignment(VAlign_Center);
	}

	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Card"));
	Card->SetBrushColor(FLinearColor(0.07f, 0.08f, 0.11f, 0.94f));
	Card->SetPadding(FMargin(36.f, 28.f));
	Card->SetVisibility(ESlateVisibility::HitTestInvisible);
	CardSize->AddChild(Card);

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Card->AddChild(Box);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	TitleText->SetJustification(ETextJustify::Center);
	TitleText->SetText(FText::FromString(TEXT("YOU DIED")));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.28f, 0.28f, 1.f)));
	TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 36));
	TitleText->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UVerticalBoxSlot* TitleSlot = Box->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TimerText"));
	TimerText->SetJustification(ETextJustify::Center);
	TimerText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.82f, 0.28f, 1.f)));
	TimerText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 48));
	TimerText->SetText(FText::FromString(TEXT("5")));
	TimerText->SetVisibility(ESlateVisibility::HitTestInvisible);
	Box->AddChildToVerticalBox(TimerText);

	WidgetTree->RootWidget = Root;
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

void UMobaRespawnHUD::Refresh()
{
	UpdateTimer();
}

void UMobaRespawnHUD::UpdateTimer()
{
	if (!TimerText)
	{
		return;
	}

	if (!OwnerCharacter || !OwnerCharacter->IsDead() || OwnerCharacter->IsShopOpen())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const float Remaining = OwnerCharacter->GetRespawnRemaining();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	TimerText->SetText(FText::FromString(FString::Printf(
		TEXT("%d"),
		FMath::Max(1, FMath::CeilToInt(Remaining)))));
}
