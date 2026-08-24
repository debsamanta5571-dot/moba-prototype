#include "MobaInventoryHUD.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/LocalPlayer.h"
#include "MobaBaseCharacter.h"
#include "MobaShopTypes.h"
#include "Styling/CoreStyle.h"

namespace
{
	FString FormatOwned(const FMobaShopOffer& Offer)
	{
		switch (Offer.Stat)
		{
		case EMobaShopStat::Damage:
		case EMobaShopStat::CooldownReduction:
		case EMobaShopStat::DamageResistance:
			return FString::Printf(TEXT("%s  +%.0f%%"), *Offer.Name, Offer.Magnitude * 100.f);
		default:
			return FString::Printf(TEXT("%s  +%.0f"), *Offer.Name, Offer.Magnitude);
		}
	}
}

UMobaInventoryHUD::UMobaInventoryHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasScriptImplementedTick = true;
}

void UMobaInventoryHUD::SetOwnerCharacter(AMobaBaseCharacter* InOwner)
{
	OwnerCharacter = InOwner;
	LastSignature.Reset();
	UpdateList();
}

void UMobaInventoryHUD::PlaceInViewport()
{
	SetIsFocusable(false);

	FGameViewportWidgetSlot ViewportSlot;
	ViewportSlot.Anchors = FAnchors(0.f, 0.5f, 0.f, 0.5f);
	ViewportSlot.Alignment = FVector2D(0.f, 0.5f);
	ViewportSlot.Offsets = FMargin(28.f, 0.f, 320.f, 420.f);
	ViewportSlot.ZOrder = 75;

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
		AddToPlayerScreen(75);
	}
}

void UMobaInventoryHUD::SetInventoryOpen(bool bOpen)
{
	SetVisibility(bOpen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bOpen)
	{
		UpdateList();
	}
}

TSharedRef<SWidget> UMobaInventoryHUD::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		RebuildList();
	}
	return Super::RebuildWidget();
}

void UMobaInventoryHUD::RebuildList()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
	Frame->SetBrushColor(FLinearColor(0.09f, 0.06f, 0.11f, 0.94f));
	Frame->SetPadding(FMargin(16.f));
	WidgetTree->RootWidget = Frame;

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
	Frame->AddChild(Root);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	Title->SetJustification(ETextJustify::Center);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.93f, 0.72f, 0.28f, 1.f)));
	Title->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
	Title->SetText(FText::FromString(TEXT("OWNED")));
	if (UVerticalBoxSlot* TitleSlot = Root->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}

	ItemList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ItemList"));
	Root->AddChildToVerticalBox(ItemList);

	EmptyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Empty"));
	EmptyText->SetJustification(ETextJustify::Center);
	EmptyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.54f, 0.44f, 1.f)));
	EmptyText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 13));
	EmptyText->SetText(FText::FromString(TEXT("Nothing purchased yet")));
	Root->AddChildToVerticalBox(EmptyText);

	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Hint"));
	Hint->SetJustification(ETextJustify::Center);
	Hint->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.5f, 0.42f, 1.f)));
	Hint->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 11));
	Hint->SetText(FText::FromString(TEXT("TAB  close")));
	if (UVerticalBoxSlot* HintSlot = Root->AddChildToVerticalBox(Hint))
	{
		HintSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	}
}

void UMobaInventoryHUD::NativeConstruct()
{
	Super::NativeConstruct();
	if (!OwnerCharacter)
	{
		OwnerCharacter = Cast<AMobaBaseCharacter>(GetOwningPlayerPawn());
	}
	RebuildList();
	PlaceInViewport();
	SetInventoryOpen(false);
}

void UMobaInventoryHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (GetVisibility() != ESlateVisibility::Collapsed)
	{
		UpdateList();
	}
}

void UMobaInventoryHUD::UpdateList()
{
	if (!ItemList || !EmptyText)
	{
		return;
	}

	TArray<FMobaShopOffer> Owned;
	if (OwnerCharacter)
	{
		Owned = OwnerCharacter->GetPurchasedOffers();
	}

	FString Signature;
	for (const FMobaShopOffer& Offer : Owned)
	{
		Signature += Offer.Name + TEXT(":");
	}
	if (Signature == LastSignature)
	{
		return;
	}
	LastSignature = Signature;

	ItemList->ClearChildren();
	EmptyText->SetVisibility(Owned.Num() == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Owned.Num() == 0)
	{
		return;
	}

	TMap<FString, int32> Counts;
	TMap<FString, FMobaShopOffer> Samples;
	for (const FMobaShopOffer& Offer : Owned)
	{
		const FString Key = Offer.Name.IsEmpty() ? TEXT("Item") : Offer.Name;
		Counts.FindOrAdd(Key)++;
		Samples.Add(Key, Offer);
	}

	for (const TPair<FString, int32>& Pair : Counts)
	{
		const FMobaShopOffer* Sample = Samples.Find(Pair.Key);
		FString Line = Sample ? FormatOwned(*Sample) : Pair.Key;
		if (Pair.Value > 1)
		{
			Line += FString::Printf(TEXT("  x%d"), Pair.Value);
		}

		UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
		Row->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.88f, 0.76f, 1.f)));
		Row->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
		Row->SetText(FText::FromString(Line));
		if (UVerticalBoxSlot* RowSlot = ItemList->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 4.f));
		}
	}
}
