#include "MobaLoadingWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UMobaLoadingWidget::PlaceInViewport()
{
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
	if (!IsInViewport())
	{
		AddToViewport(250);
	}
	SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
	SetAlignmentInViewport(FVector2D(0.f, 0.f));
	SetMessage(PendingMessage);
}

void UMobaLoadingWidget::SetMessage(const FString& Message)
{
	PendingMessage = Message.IsEmpty() ? TEXT("LOADING...") : Message;
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(PendingMessage));
	}
}

TSharedRef<SWidget> UMobaLoadingWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));

		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
		Backdrop->SetBrushColor(FLinearColor(0.004f, 0.039f, 0.075f, 1.f));
		if (UOverlaySlot* Fill = Root->AddChildToOverlay(Backdrop))
		{
			Fill->SetHorizontalAlignment(HAlign_Fill);
			Fill->SetVerticalAlignment(VAlign_Fill);
		}

		USizeBox* GoldWashSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GoldWashSize"));
		GoldWashSize->SetWidthOverride(560.f);
		UBorder* GoldWash = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GoldWash"));
		GoldWash->SetBrushColor(FLinearColor(0.45f, 0.32f, 0.08f, 0.28f));
		GoldWashSize->AddChild(GoldWash);
		if (UOverlaySlot* GoldSlot = Root->AddChildToOverlay(GoldWashSize))
		{
			GoldSlot->SetHorizontalAlignment(HAlign_Left);
			GoldSlot->SetVerticalAlignment(VAlign_Fill);
		}

		USizeBox* BronzeWashSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BronzeWashSize"));
		BronzeWashSize->SetWidthOverride(560.f);
		UBorder* BronzeWash = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BronzeWash"));
		BronzeWash->SetBrushColor(FLinearColor(0.18f, 0.12f, 0.04f, 0.32f));
		BronzeWashSize->AddChild(BronzeWash);
		if (UOverlaySlot* BronzeSlot = Root->AddChildToOverlay(BronzeWashSize))
		{
			BronzeSlot->SetHorizontalAlignment(HAlign_Right);
			BronzeSlot->SetVerticalAlignment(VAlign_Fill);
		}

		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
		TitleText->SetText(FText::FromString(PendingMessage));
		TitleText->SetJustification(ETextJustify::Center);
		TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.784f, 0.667f, 0.431f, 1.f)));
		TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 36));
		TitleText->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* Center = Root->AddChildToOverlay(TitleText))
		{
			Center->SetHorizontalAlignment(HAlign_Center);
			Center->SetVerticalAlignment(VAlign_Center);
		}

		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

void UMobaLoadingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Age += InDeltaTime;
	if (TitleText)
	{
		const float Pulse = 0.72f + 0.28f * (0.5f + 0.5f * FMath::Sin(Age * 3.2f));
		TitleText->SetRenderOpacity(Pulse);
	}
}
