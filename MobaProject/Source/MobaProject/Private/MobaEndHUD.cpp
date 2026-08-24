#include "MobaEndHUD.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "MobaBaseCharacter.h"
#include "MobaGameInstance.h"
#include "Styling/CoreStyle.h"

UMobaEndHUD::UMobaEndHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMobaEndHUD::ShowResult(bool bInVictory)
{
	bVictory = bInVictory;
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(bVictory ? TEXT("VICTORY") : TEXT("DEFEAT")));
		TitleText->SetColorAndOpacity(FSlateColor(bVictory
			? FLinearColor(0.35f, 0.82f, 0.42f, 1.f)
			: FLinearColor(0.9f, 0.28f, 0.28f, 1.f)));
	}
}

void UMobaEndHUD::ShowLoading(const FString& Message)
{
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(Message));
		TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.82f, 0.28f, 1.f)));
	}
	if (PlayAgainButton)
	{
		PlayAgainButton->SetIsEnabled(false);
		if (UWidget* Parent = PlayAgainButton->GetParent())
		{
			Parent->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (MenuButton)
	{
		MenuButton->SetIsEnabled(false);
		if (UWidget* Parent = MenuButton->GetParent())
		{
			Parent->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UMobaEndHUD::PlaceInViewport()
{
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
	if (!IsInViewport())
	{
		AddToViewport(200);
	}
	SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
	SetAlignmentInViewport(FVector2D(0.f, 0.f));
}

TSharedRef<SWidget> UMobaEndHUD::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));

		UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
		Dim->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.72f));
		Dim->SetVisibility(ESlateVisibility::Visible);
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
		Card->SetBrushColor(FLinearColor(0.07f, 0.08f, 0.11f, 0.96f));
		Card->SetPadding(FMargin(36.f, 32.f));
		Card->SetVisibility(ESlateVisibility::Visible);
		CardSize->AddChild(Card);

		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
		Card->AddChild(Box);

		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
		TitleText->SetJustification(ETextJustify::Center);
		TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 42));
		TitleText->SetText(FText::FromString(bVictory ? TEXT("VICTORY") : TEXT("DEFEAT")));
		TitleText->SetColorAndOpacity(FSlateColor(bVictory
			? FLinearColor(0.35f, 0.82f, 0.42f, 1.f)
			: FLinearColor(0.9f, 0.28f, 0.28f, 1.f)));
		TitleText->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UVerticalBoxSlot* TitleSlot = Box->AddChildToVerticalBox(TitleText))
		{
			TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));
			TitleSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		auto MakeButton = [this](const FName Name, const FString& Label, const FLinearColor& Color) -> UButton*
		{
			USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("%sSize"), *Name.ToString()));
			Size->SetHeightOverride(52.f);

			UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
			Button->SetBackgroundColor(Color);
			Button->SetVisibility(ESlateVisibility::Visible);
			Button->SetIsEnabled(true);

			UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("%sLabel"), *Name.ToString()));
			Text->SetText(FText::FromString(Label));
			Text->SetJustification(ETextJustify::Center);
			Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.93f, 0.88f, 1.f)));
			Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));
			Text->SetVisibility(ESlateVisibility::HitTestInvisible);
			Button->AddChild(Text);
			Size->AddChild(Button);
			return Button;
		};

		PlayAgainButton = MakeButton(TEXT("PlayAgain"), TEXT("Play Again"), FLinearColor(0.12f, 0.42f, 0.28f, 1.f));
		if (UVerticalBoxSlot* PlaySlot = Box->AddChildToVerticalBox(PlayAgainButton->GetParent()))
		{
			PlaySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
			PlaySlot->SetHorizontalAlignment(HAlign_Fill);
		}

		MenuButton = MakeButton(TEXT("Menu"), TEXT("Return to Main Menu"), FLinearColor(0.16f, 0.32f, 0.52f, 1.f));
		Box->AddChildToVerticalBox(MenuButton->GetParent());

		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

void UMobaEndHUD::NativeConstruct()
{
	Super::NativeConstruct();
	if (PlayAgainButton)
	{
		PlayAgainButton->OnClicked.AddUniqueDynamic(this, &UMobaEndHUD::OnPlayAgainClicked);
	}
	if (MenuButton)
	{
		MenuButton->OnClicked.AddUniqueDynamic(this, &UMobaEndHUD::OnMenuClicked);
	}
	PlaceInViewport();
}

void UMobaEndHUD::RequestTravel(bool bPlayAgain)
{
	ShowLoading(bPlayAgain ? TEXT("LOADING...") : TEXT("RETURNING TO MENU..."));

	UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>();
	APlayerController* PC = GetOwningPlayer();
	if (GI && PC && PC->HasAuthority())
	{
		if (bPlayAgain)
		{
			GI->RestartMatch();
		}
		else
		{
			GI->ReturnToMenu();
		}
		return;
	}

	if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(GetOwningPlayerPawn()))
	{
		if (bPlayAgain)
		{
			Hero->ServerRequestPlayAgain();
		}
		else
		{
			Hero->ServerRequestReturnToMenu();
		}
		return;
	}

	if (GI && !bPlayAgain)
	{
		GI->ReturnToMenu();
	}
}

void UMobaEndHUD::OnPlayAgainClicked()
{
	RequestTravel(true);
}

void UMobaEndHUD::OnMenuClicked()
{
	RequestTravel(false);
}
