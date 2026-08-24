#include "MobaGoldHUD.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "MobaBaseCharacter.h"
#include "Styling/CoreStyle.h"

UMobaGoldHUD::UMobaGoldHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasScriptImplementedTick = true;
}

void UMobaGoldHUD::SetOwnerCharacter(AMobaBaseCharacter* InOwner)
{
	OwnerCharacter = InOwner;
	UpdateGold();
}

void UMobaGoldHUD::PlaceInViewport()
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SetIsFocusable(false);

	FGameViewportWidgetSlot ViewportSlot;
	ViewportSlot.Anchors = FAnchors(1.f, 0.22f, 1.f, 0.22f);
	ViewportSlot.Alignment = FVector2D(1.f, 0.5f);
	ViewportSlot.Offsets = FMargin(-28.f, 0.f, 268.f, 52.f);
	ViewportSlot.ZOrder = 55;

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
		AddToPlayerScreen(55);
	}
	SetAnchorsInViewport(FAnchors(1.f, 0.22f, 1.f, 0.22f));
	SetAlignmentInViewport(FVector2D(1.f, 0.5f));
	SetPositionInViewport(FVector2D(-28.f, 0.f), false);
}

TSharedRef<SWidget> UMobaGoldHUD::RebuildWidget()
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

void UMobaGoldHUD::Rebuild()
{
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
	Frame->SetBrushColor(FLinearColor(0.05f, 0.06f, 0.08f, 0.92f));
	Frame->SetPadding(FMargin(14.f, 8.f));
	WidgetTree->RootWidget = Frame;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	Frame->AddChild(Row);

	BuyKeyFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("KeyFrame"));
	BuyKeyFrame->SetBrushColor(FLinearColor(0.12f, 0.1f, 0.08f, 1.f));
	BuyKeyFrame->SetPadding(FMargin(8.f, 2.f));
	BuyKeyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BuyKey"));
	BuyKeyText->SetJustification(ETextJustify::Center);
	BuyKeyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.32f, 0.28f, 1.f)));
	BuyKeyText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
	BuyKeyText->SetText(FText::FromString(TEXT("B")));
	BuyKeyFrame->AddChild(BuyKeyText);
	if (UHorizontalBoxSlot* KeySlot = Row->AddChildToHorizontalBox(BuyKeyFrame))
	{
		KeySlot->SetVerticalAlignment(VAlign_Center);
		KeySlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	}

	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
	Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.82f, 0.28f, 1.f)));
	Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
	Label->SetText(FText::FromString(TEXT("GOLD")));
	if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label))
	{
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
	}

	GoldText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GoldText"));
	GoldText->SetJustification(ETextJustify::Right);
	GoldText->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.9f, 0.45f, 1.f)));
	GoldText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
	GoldText->SetText(FText::FromString(TEXT("0")));
	if (UHorizontalBoxSlot* GoldSlot = Row->AddChildToHorizontalBox(GoldText))
	{
		GoldSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		GoldSlot->SetVerticalAlignment(VAlign_Center);
		GoldSlot->SetHorizontalAlignment(HAlign_Right);
	}
}

void UMobaGoldHUD::NativeConstruct()
{
	Super::NativeConstruct();
	if (!OwnerCharacter)
	{
		OwnerCharacter = Cast<AMobaBaseCharacter>(GetOwningPlayerPawn());
	}
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceInViewport();
	UpdateGold();
}

void UMobaGoldHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateGold();
}

void UMobaGoldHUD::UpdateGold()
{
	if (!GoldText || !OwnerCharacter)
	{
		return;
	}
	GoldText->SetText(FText::FromString(FString::Printf(
		TEXT("%d"),
		FMath::RoundToInt(OwnerCharacter->GetGold()))));

	if (BuyKeyText)
	{
		const bool bLit = OwnerCharacter->CanUseShop();
		BuyKeyText->SetColorAndOpacity(FSlateColor(bLit
			? FLinearColor(0.08f, 0.06f, 0.02f, 1.f)
			: FLinearColor(0.32f, 0.28f, 0.24f, 1.f)));
		if (BuyKeyFrame)
		{
			BuyKeyFrame->SetBrushColor(bLit
				? FLinearColor(0.98f, 0.82f, 0.22f, 1.f)
				: FLinearColor(0.12f, 0.1f, 0.08f, 1.f));
		}
	}
}
