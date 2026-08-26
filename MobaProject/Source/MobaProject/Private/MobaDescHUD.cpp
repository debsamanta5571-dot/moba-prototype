#include "MobaDescHUD.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "MobaBaseCharacter.h"
#include "MobaDescComponent.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor Cream(0.941f, 0.902f, 0.824f, 1.f);
	const FLinearColor Gold(0.784f, 0.667f, 0.431f, 1.f);
	const FLinearColor Muted(0.627f, 0.608f, 0.549f, 1.f);
}

UMobaDescHUD::UMobaDescHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasScriptImplementedTick = true;
}

void UMobaDescHUD::SetDescComponent(UMobaDescComponent* InDesc)
{
	DescComponent = InDesc;
	RebuildList();
}

void UMobaDescHUD::PlaceInViewport()
{
	SetIsFocusable(false);

	FGameViewportWidgetSlot ViewportSlot;
	ViewportSlot.Anchors = FAnchors(0.f, 0.f, 1.f, 1.f);
	ViewportSlot.Alignment = FVector2D(0.f, 0.f);
	ViewportSlot.Offsets = FMargin(0.f, 0.f, 0.f, 0.f);
	ViewportSlot.ZOrder = 80;

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
		AddToPlayerScreen(80);
	}
	SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
	SetAlignmentInViewport(FVector2D(0.f, 0.f));
}

TSharedRef<SWidget> UMobaDescHUD::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
		Dim->SetBrushColor(FLinearColor(0.004f, 0.039f, 0.075f, 0.72f));
		Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim))
		{
			DimSlot->SetHorizontalAlignment(HAlign_Fill);
			DimSlot->SetVerticalAlignment(VAlign_Fill);
		}

		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CardSize"));
		CardSize->SetWidthOverride(520.f);
		if (UOverlaySlot* CardSlot = Root->AddChildToOverlay(CardSize))
		{
			CardSlot->SetHorizontalAlignment(HAlign_Center);
			CardSlot->SetVerticalAlignment(VAlign_Center);
		}

		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Card"));
		Card->SetBrushColor(FLinearColor(0.05f, 0.06f, 0.08f, 0.96f));
		Card->SetPadding(FMargin(28.f, 24.f));
		CardSize->AddChild(Card);

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
		Card->AddChild(Column);

		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
		TitleText->SetText(FText::FromString(TEXT("Abilities")));
		TitleText->SetColorAndOpacity(FSlateColor(Gold));
		TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 22));
		if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText))
		{
			TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
		}

		ListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("List"));
		if (UVerticalBoxSlot* ListSlot = Column->AddChildToVerticalBox(ListBox))
		{
			ListSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
		}

		UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Hint"));
		Hint->SetText(FText::FromString(TEXT("TAB  close")));
		Hint->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 1.f)));
		Hint->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 12));
		Column->AddChildToVerticalBox(Hint);

		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

void UMobaDescHUD::NativeConstruct()
{
	Super::NativeConstruct();
	RebuildList();
}

void UMobaDescHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (GetVisibility() == ESlateVisibility::Collapsed || !DescComponent)
	{
		return;
	}
	const AActor* Owner = DescComponent->GetOwner();
	const AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Owner);
	const float Boost = Hero ? Hero->GetDamageModifier() : 1.f;
	if (!FMath::IsNearlyEqual(Boost, LastDamageBoost, 0.001f))
	{
		RebuildList();
	}
}

void UMobaDescHUD::RebuildList()
{
	if (!ListBox)
	{
		return;
	}
	ListBox->ClearChildren();

	FString Title = TEXT("Abilities");
	TArray<FMobaAbilityDesc> Lines;
	if (DescComponent)
	{
		if (!DescComponent->Title.IsEmpty())
		{
			Title = DescComponent->Title;
		}
		Lines = DescComponent->GetLines();
		if (const AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(DescComponent->GetOwner()))
		{
			LastDamageBoost = Hero->GetDamageModifier();
		}
	}
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(Title));
	}

	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		const FMobaAbilityDesc& Line = Lines[i];
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			NAME_None);

		if (Line.Image)
		{
			USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(
				USizeBox::StaticClass(),
				NAME_None);
			IconSize->SetWidthOverride(40.f);
			IconSize->SetHeightOverride(40.f);
			UImage* Icon = WidgetTree->ConstructWidget<UImage>(
				UImage::StaticClass(),
				NAME_None);
			Icon->SetBrushFromTexture(Line.Image, true);
			IconSize->AddChild(Icon);
			if (UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(IconSize))
			{
				IconSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
				IconSlot->SetVerticalAlignment(VAlign_Top);
			}
		}

		UTextBlock* Bullet = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			NAME_None);
		Bullet->SetText(FText::FromString(TEXT("•")));
		Bullet->SetColorAndOpacity(FSlateColor(Gold));
		Bullet->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
		if (UHorizontalBoxSlot* BulletSlot = Row->AddChildToHorizontalBox(Bullet))
		{
			BulletSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			BulletSlot->SetVerticalAlignment(VAlign_Top);
		}

		UTextBlock* Body = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			NAME_None);
		Body->SetText(FText::FromString(Line.Text.IsEmpty() ? TEXT(" ") : Line.Text));
		Body->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 1.f)));
		Body->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 15));
		Body->SetAutoWrapText(true);
		if (UHorizontalBoxSlot* BodySlot = Row->AddChildToHorizontalBox(Body))
		{
			BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BodySlot->SetVerticalAlignment(VAlign_Top);
		}

		if (UVerticalBoxSlot* RowSlot = ListBox->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		}
	}
}
