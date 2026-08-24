#include "MobaShopHUD.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/LocalPlayer.h"
#include "MobaBaseCharacter.h"
#include "MobaShopTypes.h"
#include "Styling/CoreStyle.h"

void UMobaShopBuyButton::HandleClicked()
{
	if (UMobaShopHUD* HUD = ShopHUD.Get())
	{
		HUD->RequestBuy(OfferIndex);
	}
}

namespace
{
	FString FormatMagnitude(const FMobaShopOffer& Offer)
	{
		switch (Offer.Stat)
		{
		case EMobaShopStat::Damage:
		case EMobaShopStat::CooldownReduction:
		case EMobaShopStat::DamageResistance:
			return FString::Printf(TEXT("+%.0f%%"), Offer.Magnitude * 100.f);
		default:
			return FString::Printf(TEXT("+%.0f"), Offer.Magnitude);
		}
	}
}

UMobaShopHUD::UMobaShopHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasScriptImplementedTick = true;
}

void UMobaShopHUD::SetOwnerCharacter(AMobaBaseCharacter* InOwner)
{
	OwnerCharacter = InOwner;
	RebuildOffers();
}

void UMobaShopHUD::PlaceInViewport()
{
	SetIsFocusable(true);

	FGameViewportWidgetSlot ViewportSlot;
	ViewportSlot.Anchors = FAnchors(0.5f, 0.5f, 0.5f, 0.5f);
	ViewportSlot.Alignment = FVector2D(0.5f, 0.5f);
	ViewportSlot.Offsets = FMargin(0.f, 0.f, 420.f, 460.f);
	ViewportSlot.ZOrder = 120;

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
		AddToPlayerScreen(120);
	}
	SetAnchorsInViewport(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
}

void UMobaShopHUD::SetShopOpen(bool bOpen)
{
	SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bOpen)
	{
		UpdateOffers();
	}
}

void UMobaShopHUD::RequestBuy(int32 OfferIndex)
{
	if (OwnerCharacter)
	{
		OwnerCharacter->TryBuyShopOffer(OfferIndex);
	}
}

TSharedRef<SWidget> UMobaShopHUD::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		RebuildOffers();
	}
	return Super::RebuildWidget();
}

void UMobaShopHUD::RebuildOffers()
{
	BuyButtons.Reset();
	CostTexts.Reset();

	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
	Frame->SetBrushColor(FLinearColor(0.04f, 0.045f, 0.06f, 0.94f));
	Frame->SetPadding(FMargin(16.f));
	WidgetTree->RootWidget = Frame;

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
	Frame->AddChild(Root);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	Title->SetJustification(ETextJustify::Center);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.82f, 0.28f, 1.f)));
	Title->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 22));
	Title->SetText(FText::FromString(TEXT("SHOP")));
	if (UVerticalBoxSlot* TitleSlot = Root->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	GoldText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GoldText"));
	GoldText->SetJustification(ETextJustify::Center);
	GoldText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.82f, 0.28f, 1.f)));
	GoldText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
	GoldText->SetText(FText::FromString(TEXT("Gold 0")));
	if (UVerticalBoxSlot* GoldSlot = Root->AddChildToVerticalBox(GoldText))
	{
		GoldSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.74f, 0.78f, 1.f)));
	StatusText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 11));
	StatusText->SetText(FText::FromString(TEXT("In your team's shop, or while dead")));
	if (UVerticalBoxSlot* StatusSlot = Root->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}

	OfferList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OfferList"));
	Root->AddChildToVerticalBox(OfferList);

	const TArray<FMobaShopOffer> Offers = OwnerCharacter
		? OwnerCharacter->GetShopOffers()
		: TArray<FMobaShopOffer>();

	for (int32 i = 0; i < Offers.Num(); ++i)
	{
		const FMobaShopOffer& Offer = Offers[i];

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			*FString::Printf(TEXT("Row%d"), i));

		UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			*FString::Printf(TEXT("Name%d"), i));
		Name->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.93f, 0.9f, 1.f)));
		Name->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 13));
		Name->SetText(FText::FromString(FString::Printf(
			TEXT("%s  %s"),
			*Offer.Name,
			*FormatMagnitude(Offer))));
		if (UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(Name))
		{
			NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			NameSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* Cost = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			*FString::Printf(TEXT("Cost%d"), i));
		Cost->SetJustification(ETextJustify::Right);
		Cost->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.82f, 0.28f, 1.f)));
		Cost->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 13));
		Cost->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), Offer.Cost)));
		if (UHorizontalBoxSlot* CostSlot = Row->AddChildToHorizontalBox(Cost))
		{
			CostSlot->SetPadding(FMargin(8.f, 0.f));
			CostSlot->SetVerticalAlignment(VAlign_Center);
		}

		UMobaShopBuyButton* Buy = WidgetTree->ConstructWidget<UMobaShopBuyButton>(
			UMobaShopBuyButton::StaticClass(),
			*FString::Printf(TEXT("Buy%d"), i));
		Buy->OfferIndex = i;
		Buy->ShopHUD = this;
		Buy->OnClicked.AddDynamic(Buy, &UMobaShopBuyButton::HandleClicked);
		Buy->SetBackgroundColor(FLinearColor(0.18f, 0.42f, 0.22f, 1.f));

		UTextBlock* BuyLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			*FString::Printf(TEXT("BuyLabel%d"), i));
		BuyLabel->SetJustification(ETextJustify::Center);
		BuyLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		BuyLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
		BuyLabel->SetText(FText::FromString(TEXT("BUY")));
		Buy->AddChild(BuyLabel);

		USizeBox* BuySize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("BuySize%d"), i));
		BuySize->SetWidthOverride(72.f);
		BuySize->SetHeightOverride(28.f);
		BuySize->AddChild(Buy);
		if (UHorizontalBoxSlot* BuySlot = Row->AddChildToHorizontalBox(BuySize))
		{
			BuySlot->SetVerticalAlignment(VAlign_Center);
		}

		if (UVerticalBoxSlot* RowSlot = OfferList->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 4.f));
		}

		BuyButtons.Add(Buy);
		CostTexts.Add(Cost);
	}

	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Hint"));
	Hint->SetJustification(ETextJustify::Center);
	Hint->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.57f, 0.6f, 1.f)));
	Hint->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 11));
	Hint->SetText(FText::FromString(TEXT("B  close")));
	if (UVerticalBoxSlot* HintSlot = Root->AddChildToVerticalBox(Hint))
	{
		HintSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	}
}

void UMobaShopHUD::NativeConstruct()
{
	Super::NativeConstruct();
	if (!OwnerCharacter)
	{
		OwnerCharacter = Cast<AMobaBaseCharacter>(GetOwningPlayerPawn());
	}
	RebuildOffers();
	PlaceInViewport();
	SetShopOpen(false);
	UpdateOffers();
}

void UMobaShopHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (GetVisibility() != ESlateVisibility::Collapsed)
	{
		UpdateOffers();
	}
}

void UMobaShopHUD::UpdateOffers()
{
	if (!OwnerCharacter || !GoldText || !StatusText)
	{
		return;
	}

	GoldText->SetText(FText::FromString(FString::Printf(
		TEXT("Gold  %d"),
		FMath::RoundToInt(OwnerCharacter->GetGold()))));

	const bool bCanUse = OwnerCharacter->CanUseShop();
	if (bCanUse)
	{
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.82f, 0.45f, 1.f)));
		StatusText->SetText(FText::FromString(
			OwnerCharacter->IsDead() ? TEXT("Dead. Shop available") : TEXT("In your team's shop")));
	}
	else
	{
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.4f, 0.35f, 1.f)));
		StatusText->SetText(FText::FromString(TEXT("Walk into your shop, or shop while dead")));
	}

	const TArray<FMobaShopOffer>& Offers = OwnerCharacter->GetShopOffers();
	for (int32 i = 0; i < BuyButtons.Num(); ++i)
	{
		if (!BuyButtons[i] || !Offers.IsValidIndex(i))
		{
			continue;
		}
		const bool bCanBuy = OwnerCharacter->CanBuyShopOffer(i);
		BuyButtons[i]->SetIsEnabled(bCanBuy);
		BuyButtons[i]->SetBackgroundColor(bCanBuy
			? FLinearColor(0.18f, 0.42f, 0.22f, 1.f)
			: FLinearColor(0.18f, 0.18f, 0.2f, 1.f));
		if (CostTexts.IsValidIndex(i) && CostTexts[i])
		{
			const bool bAfford = OwnerCharacter->GetGold() + 0.01f >= Offers[i].Cost;
			CostTexts[i]->SetColorAndOpacity(FSlateColor(bAfford
				? FLinearColor(0.95f, 0.82f, 0.28f, 1.f)
				: FLinearColor(0.82f, 0.32f, 0.28f, 1.f)));
		}
	}
}
