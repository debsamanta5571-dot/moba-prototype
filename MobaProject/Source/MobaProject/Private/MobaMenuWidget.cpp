#include "MobaMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "MobaGameInstance.h"
#include "Styling/CoreStyle.h"

namespace
{
	UTextBlock* MakeLabel(UWidgetTree* Tree, const FName Name, const FString& Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetText(FText::FromString(Text));
		Label->SetColorAndOpacity(FSlateColor(Color));
		Label->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", Size);
		Label->SetFont(Font);
		return Label;
	}

	void ColorButton(UButton* Button, const FLinearColor& Color)
	{
		Button->SetBackgroundColor(Color);
	}

	UVerticalBoxSlot* AddPadded(UVerticalBox* Box, UWidget* Child, float Bottom)
	{
		UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Child);
		Slot->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
		Slot->SetHorizontalAlignment(HAlign_Fill);
		return Slot;
	}
}

TSharedRef<SWidget> UMobaMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		const FLinearColor Cream(0.95f, 0.93f, 0.88f, 1.f);
		const FLinearColor Muted(0.72f, 0.74f, 0.78f, 1.f);
		const FLinearColor HostGreen(0.12f, 0.42f, 0.28f, 1.f);
		const FLinearColor JoinBlue(0.16f, 0.32f, 0.52f, 1.f);

		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));

		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
		Backdrop->SetBrushColor(FLinearColor(0.03f, 0.035f, 0.045f, 1.f));
		Backdrop->SetPadding(FMargin(0.f));
		if (UOverlaySlot* Fill = Root->AddChildToOverlay(Backdrop))
		{
			Fill->SetHorizontalAlignment(HAlign_Fill);
			Fill->SetVerticalAlignment(VAlign_Fill);
		}

		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CardSize"));
		CardSize->SetWidthOverride(420.f);
		if (UOverlaySlot* Center = Root->AddChildToOverlay(CardSize))
		{
			Center->SetHorizontalAlignment(HAlign_Center);
			Center->SetVerticalAlignment(VAlign_Center);
		}

		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Card"));
		Card->SetBrushColor(FLinearColor(0.07f, 0.08f, 0.11f, 1.f));
		Card->SetPadding(FMargin(40.f, 36.f));
		CardSize->AddChild(Card);

		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
		Card->AddChild(Box);

		AddPadded(Box, MakeLabel(WidgetTree, TEXT("Title"), TEXT("MOBA"), 36, Cream), 8.f);
		AddPadded(Box, MakeLabel(WidgetTree, TEXT("Subtitle"), TEXT("Listen server"), 14, Muted), 28.f);

		HostButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Host"));
		ColorButton(HostButton, HostGreen);
		HostButton->AddChild(MakeLabel(WidgetTree, TEXT("HostLabel"), TEXT("Host"), 18, Cream));
		if (UVerticalBoxSlot* HostSlot = AddPadded(Box, HostButton, 20.f))
		{
			HostSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
		}

		AddPadded(Box, MakeLabel(WidgetTree, TEXT("JoinHint"), TEXT("Join address"), 13, Muted), 6.f);

		AddressBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Address"));
		AddressBox->SetText(FText::FromString(TEXT("127.0.0.1")));
		AddressBox->SetForegroundColor(FLinearColor::Black);
		AddPadded(Box, AddressBox, 12.f);

		JoinButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Join"));
		ColorButton(JoinButton, JoinBlue);
		JoinButton->AddChild(MakeLabel(WidgetTree, TEXT("JoinLabel"), TEXT("Join"), 18, Cream));
		Box->AddChildToVerticalBox(JoinButton);

		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

void UMobaMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
	SetAlignmentInViewport(FVector2D(0.f, 0.f));

	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &UMobaMenuWidget::OnHostClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &UMobaMenuWidget::OnJoinClicked);
	}
}

void UMobaMenuWidget::OnHostClicked()
{
	if (UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance()))
	{
		GI->HostGame();
	}
}

void UMobaMenuWidget::OnJoinClicked()
{
	UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	const FString Address = AddressBox ? AddressBox->GetText().ToString() : TEXT("127.0.0.1");
	GI->JoinGame(Address);
}
