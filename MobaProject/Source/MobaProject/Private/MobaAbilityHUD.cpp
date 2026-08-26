#include "MobaAbilityHUD.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetConnection.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "MobaBaseCharacter.h"
#include "MobaGameplayAbility.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor SlotColors[4] = {
		FLinearColor(0.78f, 0.28f, 0.22f, 1.f),
		FLinearColor(0.22f, 0.48f, 0.82f, 1.f),
		FLinearColor(0.86f, 0.72f, 0.22f, 1.f),
		FLinearColor(0.28f, 0.68f, 0.40f, 1.f)
	};
}

UMobaAbilityHUD::UMobaAbilityHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasScriptImplementedTick = true;
}

void UMobaAbilityHUD::SetOwnerCharacter(AMobaBaseCharacter* InOwner)
{
	OwnerCharacter = InOwner;
	FillAbilityIcons();
	UpdateSlots();
}

void UMobaAbilityHUD::PlaceInViewport()
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SetIsFocusable(false);

	FGameViewportWidgetSlot ViewportSlot;
	ViewportSlot.Anchors = FAnchors(0.f, 0.f, 1.f, 1.f);
	ViewportSlot.Alignment = FVector2D(0.f, 0.f);
	ViewportSlot.Offsets = FMargin(0.f, 0.f, 0.f, 0.f);
	ViewportSlot.ZOrder = 50;

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
		AddToPlayerScreen(50);
	}
	SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
	SetAlignmentInViewport(FVector2D(0.f, 0.f));
	SetPositionInViewport(FVector2D(0.f, 0.f), false);
}

TSharedRef<SWidget> UMobaAbilityHUD::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		RebuildSlots();
	}
	return Super::RebuildWidget();
}

void UMobaAbilityHUD::RebuildSlots()
{
	Icons.Reset();
	CooldownBars.Reset();
	TimeTexts.Reset();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UVerticalBox* HudStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HudStack"));
	if (UOverlaySlot* HudSlot = Root->AddChildToOverlay(HudStack))
	{
		HudSlot->SetHorizontalAlignment(HAlign_Right);
		HudSlot->SetVerticalAlignment(VAlign_Bottom);
		HudSlot->SetPadding(FMargin(0.f, 0.f, 40.f, 32.f));
	}

	NoticeFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NoticeFrame"));
	NoticeFrame->SetBrushColor(FLinearColor(0.08f, 0.05f, 0.02f, 0.92f));
	NoticeFrame->SetPadding(FMargin(28.f, 12.f));
	NoticeFrame->SetVisibility(ESlateVisibility::Collapsed);
	NoticeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NoticeText"));
	NoticeText->SetJustification(ETextJustify::Center);
	NoticeText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.82f, 0.28f, 1.f)));
	NoticeText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 22));
	NoticeText->SetText(FText::GetEmpty());
	NoticeFrame->AddChild(NoticeText);
	if (UOverlaySlot* NoticeSlot = Root->AddChildToOverlay(NoticeFrame))
	{
		NoticeSlot->SetHorizontalAlignment(HAlign_Center);
		NoticeSlot->SetVerticalAlignment(VAlign_Center);
	}

	PingFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PingFrame"));
	PingFrame->SetBrushColor(FLinearColor(0.05f, 0.06f, 0.08f, 0.88f));
	PingFrame->SetPadding(FMargin(12.f, 6.f));
	PingFrame->SetVisibility(ESlateVisibility::Collapsed);
	PingText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PingText"));
	PingText->SetJustification(ETextJustify::Right);
	PingText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.67f, 0.43f, 1.f)));
	PingText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
	PingText->SetText(FText::FromString(TEXT("-- ms")));
	PingText->SetVisibility(ESlateVisibility::HitTestInvisible);
	PingFrame->AddChild(PingText);
	if (UOverlaySlot* PingSlot = Root->AddChildToOverlay(PingFrame))
	{
		PingSlot->SetHorizontalAlignment(HAlign_Right);
		PingSlot->SetVerticalAlignment(VAlign_Top);
		PingSlot->SetPadding(FMargin(0.f, 24.f, 28.f, 0.f));
	}

	USizeBox* HealthSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HealthSize"));
	HealthSize->SetWidthOverride(384.f);
	HealthSize->SetHeightOverride(22.f);

	UOverlay* HealthOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HealthOverlay"));
	HealthSize->AddChild(HealthOverlay);

	UBorder* HealthFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HealthFrame"));
	HealthFrame->SetBrushColor(FLinearColor(0.05f, 0.06f, 0.08f, 0.92f));
	HealthFrame->SetPadding(FMargin(2.f));
	if (UOverlaySlot* FrameSlot = HealthOverlay->AddChildToOverlay(HealthFrame))
	{
		FrameSlot->SetHorizontalAlignment(HAlign_Fill);
		FrameSlot->SetVerticalAlignment(VAlign_Fill);
	}

	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	HealthBar->SetFillColorAndOpacity(FLinearColor(0.2f, 0.8f, 0.25f, 1.f));
	HealthBar->SetPercent(1.f);
	HealthFrame->AddChild(HealthBar);

	HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
	HealthText->SetJustification(ETextJustify::Center);
	HealthText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.93f, 0.88f, 1.f)));
	HealthText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
	HealthText->SetText(FText::FromString(TEXT("100 / 100")));
	if (UOverlaySlot* TextSlot = HealthOverlay->AddChildToOverlay(HealthText))
	{
		TextSlot->SetHorizontalAlignment(HAlign_Center);
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}

	StatusFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatusFrame"));
	StatusFrame->SetBrushColor(FLinearColor(0.08f, 0.03f, 0.14f, 0.94f));
	StatusFrame->SetPadding(FMargin(10.f, 3.f));
	StatusFrame->SetVisibility(ESlateVisibility::Collapsed);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.72f, 1.f, 1.f)));
	StatusText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 13));
	StatusText->SetText(FText::GetEmpty());
	StatusFrame->AddChild(StatusText);
	if (UOverlaySlot* StatusSlot = HealthOverlay->AddChildToOverlay(StatusFrame))
	{
		StatusSlot->SetHorizontalAlignment(HAlign_Center);
		StatusSlot->SetVerticalAlignment(VAlign_Top);
		StatusSlot->SetPadding(FMargin(0.f, -22.f, 0.f, 0.f));
	}

	if (UVerticalBoxSlot* HealthSlot = HudStack->AddChildToVerticalBox(HealthSize))
	{
		HealthSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
		HealthSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	USizeBox* EnergySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EnergySize"));
	EnergySize->SetWidthOverride(384.f);
	EnergySize->SetHeightOverride(18.f);

	UOverlay* EnergyOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("EnergyOverlay"));
	EnergySize->AddChild(EnergyOverlay);

	UBorder* EnergyFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EnergyFrame"));
	EnergyFrame->SetBrushColor(FLinearColor(0.05f, 0.06f, 0.08f, 0.92f));
	EnergyFrame->SetPadding(FMargin(2.f));
	if (UOverlaySlot* EnergyFrameSlot = EnergyOverlay->AddChildToOverlay(EnergyFrame))
	{
		EnergyFrameSlot->SetHorizontalAlignment(HAlign_Fill);
		EnergyFrameSlot->SetVerticalAlignment(VAlign_Fill);
	}

	EnergyBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("EnergyBar"));
	EnergyBar->SetFillColorAndOpacity(FLinearColor(0.2f, 0.45f, 0.95f, 1.f));
	EnergyBar->SetPercent(1.f);
	EnergyFrame->AddChild(EnergyBar);

	EnergyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnergyText"));
	EnergyText->SetJustification(ETextJustify::Center);
	EnergyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.93f, 0.88f, 1.f)));
	EnergyText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 11));
	EnergyText->SetText(FText::FromString(TEXT("100 / 100")));
	if (UOverlaySlot* EnergyTextSlot = EnergyOverlay->AddChildToOverlay(EnergyText))
	{
		EnergyTextSlot->SetHorizontalAlignment(HAlign_Center);
		EnergyTextSlot->SetVerticalAlignment(VAlign_Center);
	}

	if (UVerticalBoxSlot* EnergySlot = HudStack->AddChildToVerticalBox(EnergySize))
	{
		EnergySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		EnergySlot->SetHorizontalAlignment(HAlign_Fill);
	}

	SlotRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	if (UVerticalBoxSlot* RowSlot = HudStack->AddChildToVerticalBox(SlotRow))
	{
		RowSlot->SetHorizontalAlignment(HAlign_Right);
	}

	UTextBlock* KeysHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("KeysHint"));
	KeysHint->SetJustification(ETextJustify::Right);
	KeysHint->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 1.f)));
	KeysHint->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 12));
	KeysHint->SetText(FText::FromString(TEXT("TAB  abilities    I  items")));
	if (UVerticalBoxSlot* KeysSlot = HudStack->AddChildToVerticalBox(KeysHint))
	{
		KeysSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		KeysSlot->SetHorizontalAlignment(HAlign_Right);
	}

	FillAbilityIcons();
}

void UMobaAbilityHUD::FillAbilityIcons()
{
	if (!WidgetTree || !SlotRow)
	{
		return;
	}

	SlotRow->ClearChildren();
	Icons.Reset();
	CooldownBars.Reset();
	TimeTexts.Reset();

	const int32 NumSlots = OwnerCharacter ? OwnerCharacter->GetAbilitySlotCount() : 4;
	for (int32 i = 0; i < NumSlots; ++i)
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			*FString::Printf(TEXT("Slot%d"), i));

		USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("Size%d"), i));
		IconSize->SetWidthOverride(72.f);
		IconSize->SetHeightOverride(72.f);

		UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(
			UOverlay::StaticClass(),
			*FString::Printf(TEXT("Overlay%d"), i));
		IconSize->AddChild(Overlay);

		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			*FString::Printf(TEXT("Frame%d"), i));
		Frame->SetBrushColor(FLinearColor(0.05f, 0.06f, 0.08f, 0.92f));
		Frame->SetPadding(FMargin(3.f));
		if (UOverlaySlot* FrameSlot = Overlay->AddChildToOverlay(Frame))
		{
			FrameSlot->SetHorizontalAlignment(HAlign_Fill);
			FrameSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UImage* Icon = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			*FString::Printf(TEXT("Icon%d"), i));
		if (const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush"))
		{
			Icon->SetBrush(*White);
		}
		Icon->SetColorAndOpacity(SlotColors[i % 4]);
		if (UOverlaySlot* IconSlot = Overlay->AddChildToOverlay(Icon))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Fill);
			IconSlot->SetVerticalAlignment(VAlign_Fill);
			IconSlot->SetPadding(FMargin(3.f));
		}

		UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(
			UProgressBar::StaticClass(),
			*FString::Printf(TEXT("Bar%d"), i));
		Bar->SetFillColorAndOpacity(FLinearColor(0.02f, 0.02f, 0.03f, 0.72f));
		Bar->SetPercent(0.f);
		if (UOverlaySlot* BarSlot = Overlay->AddChildToOverlay(Bar))
		{
			BarSlot->SetHorizontalAlignment(HAlign_Fill);
			BarSlot->SetVerticalAlignment(VAlign_Fill);
			BarSlot->SetPadding(FMargin(3.f));
		}

		UTextBlock* Time = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			*FString::Printf(TEXT("Time%d"), i));
		Time->SetJustification(ETextJustify::Center);
		Time->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.93f, 0.88f, 1.f)));
		Time->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
		Time->SetText(FText::GetEmpty());
		if (UOverlaySlot* TimeSlot = Overlay->AddChildToOverlay(Time))
		{
			TimeSlot->SetHorizontalAlignment(HAlign_Center);
			TimeSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* Key = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			*FString::Printf(TEXT("Key%d"), i));
		Key->SetJustification(ETextJustify::Center);
		Key->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.74f, 0.78f, 1.f)));
		Key->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 11));
		const FString KeyLabel = OwnerCharacter
			? OwnerCharacter->GetAbilityKeyLabel(i)
			: FString::FromInt(i + 1);
		Key->SetText(FText::FromString(KeyLabel));

		Column->AddChildToVerticalBox(IconSize);
		Column->AddChildToVerticalBox(Key);

		if (UHorizontalBoxSlot* BoxSlot = SlotRow->AddChildToHorizontalBox(Column))
		{
			BoxSlot->SetPadding(FMargin(6.f, 0.f));
			BoxSlot->SetVerticalAlignment(VAlign_Bottom);
		}

		Icons.Add(Icon);
		CooldownBars.Add(Bar);
		TimeTexts.Add(Time);
	}
}

void UMobaAbilityHUD::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceInViewport();
	UpdateStatus();
	UpdateHealth();
	UpdateEnergy();
	UpdateSlots();
	UpdatePing();
}

void UMobaAbilityHUD::ShowNotice(const FString& Message)
{
	if (!NoticeFrame || !NoticeText)
	{
		return;
	}
	NoticeText->SetText(FText::FromString(Message));
	NoticeFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	NoticeUntilTime = GetWorld() ? GetWorld()->GetTimeSeconds() + 1.4f : 0.f;
}

void UMobaAbilityHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IsValid(OwnerCharacter))
	{
		return;
	}
	if (NoticeFrame && NoticeUntilTime > 0.f && GetWorld() && GetWorld()->GetTimeSeconds() >= NoticeUntilTime)
	{
		NoticeFrame->SetVisibility(ESlateVisibility::Collapsed);
		NoticeUntilTime = 0.f;
	}
	UpdateStatus();
	UpdateHealth();
	UpdateEnergy();
	UpdateSlots();
	UpdatePing();
}

void UMobaAbilityHUD::UpdatePing()
{
	if (PingFrame)
	{
		PingFrame->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMobaAbilityHUD::UpdateHealth()
{
	if (!HealthBar || !HealthText || !OwnerCharacter)
	{
		return;
	}

	const float Health = OwnerCharacter->GetHealth();
	const float MaxHealth = OwnerCharacter->GetMaxHealth();
	HealthBar->SetFillColorAndOpacity(FLinearColor(0.2f, 0.8f, 0.25f, 1.f));
	HealthBar->SetPercent(MaxHealth > 0.f ? Health / MaxHealth : 0.f);
	HealthText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(Health), FMath::RoundToInt(MaxHealth))));
}

void UMobaAbilityHUD::UpdateStatus()
{
	if (!StatusText || !OwnerCharacter)
	{
		return;
	}

	if (!StatusFrame || !StatusText)
	{
		return;
	}

	if (OwnerCharacter->IsStunned())
	{
		StatusText->SetText(FText::FromString(TEXT("Stunned")));
		StatusFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else if (OwnerCharacter->IsSlowed())
	{
		StatusText->SetText(FText::FromString(TEXT("Slowed")));
		StatusFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		StatusText->SetText(FText::GetEmpty());
		StatusFrame->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMobaAbilityHUD::UpdateEnergy()
{
	if (!EnergyBar || !EnergyText || !OwnerCharacter)
	{
		return;
	}

	const float Energy = OwnerCharacter->GetEnergy();
	const float MaxEnergy = OwnerCharacter->GetMaxEnergy();
	EnergyBar->SetPercent(MaxEnergy > 0.f ? Energy / MaxEnergy : 0.f);
	EnergyText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(Energy), FMath::RoundToInt(MaxEnergy))));
}

void UMobaAbilityHUD::UpdateSlots()
{
	if (!OwnerCharacter || Icons.Num() == 0)
	{
		return;
	}

	const bool bDead = OwnerCharacter->IsDead();
	const int32 Num = Icons.Num();
	for (int32 i = 0; i < Num; ++i)
	{
		UTexture2D* IconTex = nullptr;
		float Remaining = 0.f;
		float Duration = 0.f;
		OwnerCharacter->GetAbilityHudInfo(i, IconTex, Remaining, Duration);

		if (IconTex)
		{
			Icons[i]->SetBrushFromTexture(IconTex);
			Icons[i]->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			Icons[i]->SetColorAndOpacity(SlotColors[i % 4]);
		}

		const bool bOnCooldown = Remaining > 0.05f && Duration > 0.f;
		if (bOnCooldown)
		{
			Icons[i]->SetColorAndOpacity(Icons[i]->GetColorAndOpacity() * FLinearColor(0.45f, 0.45f, 0.45f, 1.f));
			CooldownBars[i]->SetPercent(FMath::Clamp(Remaining / Duration, 0.f, 1.f));
			CooldownBars[i]->SetVisibility(ESlateVisibility::HitTestInvisible);
			TimeTexts[i]->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Remaining)));
		}
		else
		{
			CooldownBars[i]->SetPercent(0.f);
			CooldownBars[i]->SetVisibility(ESlateVisibility::Hidden);
			TimeTexts[i]->SetText(FText::GetEmpty());
		}

		if (bDead)
		{
			Icons[i]->SetColorAndOpacity(FLinearColor(0.25f, 0.25f, 0.25f, 0.7f));
		}
	}
}
