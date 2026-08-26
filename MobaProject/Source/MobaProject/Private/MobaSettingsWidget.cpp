#include "MobaSettingsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "InputCoreTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "MobaBaseCharacter.h"
#include "MobaGameInstance.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

void UMobaSettingsWidget::PlaceInViewport()
{
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
	if (!IsInViewport())
	{
		AddToViewport(180);
	}
	SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
	SetAlignmentInViewport(FVector2D(0.f, 0.f));
}

TSharedRef<SWidget> UMobaSettingsWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		const FLinearColor Cream(0.941f, 0.902f, 0.824f, 1.f);
		const FLinearColor Muted(0.627f, 0.608f, 0.549f, 1.f);
		const FLinearColor Gold(0.784f, 0.667f, 0.431f, 1.f);
		const FLinearColor Ink(0.004f, 0.039f, 0.075f, 1.f);

		auto MakeRoundBrush = [](const FLinearColor& Color, float Radius)
		{
			FSlateBrush Brush;
			if (const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush"))
			{
				Brush = *White;
			}
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.TintColor = FSlateColor(Color);
			Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
			Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			Brush.OutlineSettings.Width = 0.f;
			return Brush;
		};

		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));

		UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
		Dim->SetBrushColor(FLinearColor(0.004f, 0.039f, 0.075f, 0.82f));
		if (UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim))
		{
			DimSlot->SetHorizontalAlignment(HAlign_Fill);
			DimSlot->SetVerticalAlignment(VAlign_Fill);
		}

		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CardSize"));
		CardSize->SetWidthOverride(520.f);
		if (UOverlaySlot* Center = Root->AddChildToOverlay(CardSize))
		{
			Center->SetHorizontalAlignment(HAlign_Center);
			Center->SetVerticalAlignment(VAlign_Center);
		}

		UBorder* Hairline = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Hairline"));
		Hairline->SetBrush(MakeRoundBrush(FLinearColor(0.784f, 0.667f, 0.431f, 1.f), 18.f));
		Hairline->SetPadding(FMargin(2.f));
		CardSize->AddChild(Hairline);

		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Card"));
		Card->SetBrush(MakeRoundBrush(FLinearColor(0.118f, 0.137f, 0.157f, 0.98f), 16.f));
		Card->SetPadding(FMargin(40.f, 36.f));
		Hairline->AddChild(Card);

		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
		Card->AddChild(Box);

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
		Title->SetText(FText::FromString(TEXT("SETTINGS")));
		Title->SetJustification(ETextJustify::Center);
		Title->SetColorAndOpacity(FSlateColor(Gold));
		Title->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 28));
		Title->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UVerticalBoxSlot* TitleSlot = Box->AddChildToVerticalBox(Title))
		{
			TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));
			TitleSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		UHorizontalBox* SoundRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SoundRow"));
		UTextBlock* SoundLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SoundLabel"));
		SoundLabel->SetText(FText::FromString(TEXT("Sound")));
		SoundLabel->SetColorAndOpacity(FSlateColor(Muted));
		SoundLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
		SoundLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UHorizontalBoxSlot* SoundSlot = SoundRow->AddChildToHorizontalBox(SoundLabel))
		{
			SoundSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			SoundSlot->SetVerticalAlignment(VAlign_Center);
		}

		VolumeValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SoundValue"));
		VolumeValueText->SetColorAndOpacity(FSlateColor(Cream));
		VolumeValueText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
		VolumeValueText->SetJustification(ETextJustify::Right);
		VolumeValueText->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UHorizontalBoxSlot* ValueSlot = SoundRow->AddChildToHorizontalBox(VolumeValueText))
		{
			ValueSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* SoundRowSlot = Box->AddChildToVerticalBox(SoundRow))
		{
			SoundRowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
			SoundRowSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		USizeBox* SliderSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SliderSize"));
		SliderSize->SetHeightOverride(28.f);
		VolumeSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("SoundSlider"));
		VolumeSlider->SetMinValue(0.f);
		VolumeSlider->SetMaxValue(1.f);
		VolumeSlider->SetStepSize(0.01f);
		VolumeSlider->SetValue(0.5f);
		VolumeSlider->SetSliderBarColor(FLinearColor(0.784f, 0.667f, 0.431f, 1.f));
		VolumeSlider->SetSliderHandleColor(Cream);
		SliderSize->AddChild(VolumeSlider);
		if (UVerticalBoxSlot* SliderSlot = Box->AddChildToVerticalBox(SliderSize))
		{
			SliderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 22.f));
			SliderSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		UTextBlock* GraphicsLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GraphicsLabel"));
		GraphicsLabel->SetText(FText::FromString(TEXT("Graphics")));
		GraphicsLabel->SetColorAndOpacity(FSlateColor(Muted));
		GraphicsLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
		GraphicsLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UVerticalBoxSlot* GraphicsHint = Box->AddChildToVerticalBox(GraphicsLabel))
		{
			GraphicsHint->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
			GraphicsHint->SetHorizontalAlignment(HAlign_Fill);
		}

		UHorizontalBox* GraphicsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("GraphicsRow"));
		const TCHAR* PresetNames[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic") };
		GraphicsButtons.Reset();
		GraphicsLabels.Reset();
		for (int32 i = 0; i < 4; ++i)
		{
			USizeBox* BtnSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("GfxSize%d"), i));
			BtnSize->SetHeightOverride(40.f);
			UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("Gfx%d"), i));
			UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("GfxLabel%d"), i));
			BtnLabel->SetText(FText::FromString(PresetNames[i]));
			BtnLabel->SetJustification(ETextJustify::Center);
			BtnLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 13));
			BtnLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
			Btn->AddChild(BtnLabel);
			BtnSize->AddChild(Btn);
			if (UHorizontalBoxSlot* BtnSlot = GraphicsRow->AddChildToHorizontalBox(BtnSize))
			{
				BtnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				BtnSlot->SetPadding(FMargin(i == 0 ? 0.f : 6.f, 0.f, 0.f, 0.f));
			}
			GraphicsButtons.Add(Btn);
			GraphicsLabels.Add(BtnLabel);
		}
		if (UVerticalBoxSlot* GraphicsSlot = Box->AddChildToVerticalBox(GraphicsRow))
		{
			GraphicsSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));
			GraphicsSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		auto MakeFooterButton = [&](const FName Name, const FString& Label, const FLinearColor& Color, const FLinearColor& TextColor) -> UButton*
		{
			USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("%sSize"), *Name.ToString()));
			Size->SetHeightOverride(52.f);
			UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
			FButtonStyle Style = Button->GetStyle();
			Style.Normal = MakeRoundBrush(Color, 10.f);
			Style.Hovered = MakeRoundBrush(Color + FLinearColor(0.08f, 0.10f, 0.10f, 0.f), 10.f);
			Style.Pressed = MakeRoundBrush(Color * 0.82f, 10.f);
			Style.NormalPadding = FMargin(0.f);
			Style.PressedPadding = FMargin(0.f);
			Button->SetStyle(Style);
			UTextBlock* ButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("%sLabel"), *Name.ToString()));
			ButtonLabel->SetText(FText::FromString(Label));
			ButtonLabel->SetJustification(ETextJustify::Center);
			ButtonLabel->SetColorAndOpacity(FSlateColor(TextColor));
			ButtonLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));
			ButtonLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
			Button->AddChild(ButtonLabel);
			Size->AddChild(Button);
			return Button;
		};

		BackButton = MakeFooterButton(TEXT("Back"), TEXT("Back"), FLinearColor(0.784f, 0.608f, 0.235f, 1.f), Ink);
		if (UVerticalBoxSlot* BackSlot = Box->AddChildToVerticalBox(BackButton->GetParent()))
		{
			BackSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
			BackSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		MenuButton = MakeFooterButton(TEXT("Menu"), TEXT("Main Menu"), FLinearColor(0.471f, 0.353f, 0.157f, 1.f), Cream);
		if (UVerticalBoxSlot* MenuSlot = Box->AddChildToVerticalBox(MenuButton->GetParent()))
		{
			MenuSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
			MenuSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		QuitButton = MakeFooterButton(TEXT("Quit"), TEXT("Quit"), FLinearColor(0.357f, 0.353f, 0.337f, 1.f), Cream);
		Box->AddChildToVerticalBox(QuitButton->GetParent());

		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

void UMobaSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	PlaceInViewport();

	if (VolumeSlider)
	{
		VolumeSlider->OnValueChanged.AddUniqueDynamic(this, &UMobaSettingsWidget::OnVolumeChanged);
		float Volume = 0.5f;
		if (const UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
		{
			Volume = GI->GetMasterVolume();
		}
		VolumeSlider->SetValue(Volume);
		UpdateVolumeLabel(Volume);
	}
	if (GraphicsButtons.Num() >= 4)
	{
		GraphicsButtons[0]->OnClicked.AddUniqueDynamic(this, &UMobaSettingsWidget::OnGraphicsLow);
		GraphicsButtons[1]->OnClicked.AddUniqueDynamic(this, &UMobaSettingsWidget::OnGraphicsMedium);
		GraphicsButtons[2]->OnClicked.AddUniqueDynamic(this, &UMobaSettingsWidget::OnGraphicsHigh);
		GraphicsButtons[3]->OnClicked.AddUniqueDynamic(this, &UMobaSettingsWidget::OnGraphicsEpic);
	}
	RefreshGraphicsButtons();
	if (BackButton)
	{
		BackButton->OnClicked.AddUniqueDynamic(this, &UMobaSettingsWidget::OnBackClicked);
	}
	if (MenuButton)
	{
		MenuButton->OnClicked.AddUniqueDynamic(this, &UMobaSettingsWidget::OnMenuClicked);
		bool bOnMenu = false;
		if (const UWorld* World = GetWorld())
		{
			bOnMenu = World->GetMapName().Contains(TEXT("MobaMenu"), ESearchCase::IgnoreCase);
		}
		UWidget* MenuRow = MenuButton->GetParent() ? MenuButton->GetParent() : MenuButton;
		MenuRow->SetVisibility(bOnMenu ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddUniqueDynamic(this, &UMobaSettingsWidget::OnQuitClicked);
	}

	SetKeyboardFocus();
}

void UMobaSettingsWidget::UpdateVolumeLabel(float Volume)
{
	if (VolumeValueText)
	{
		VolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Volume * 100.f))));
	}
}

void UMobaSettingsWidget::OnVolumeChanged(float Value)
{
	if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
	{
		GI->SetMasterVolume(Value);
	}
	UpdateVolumeLabel(Value);
}

void UMobaSettingsWidget::OnGraphicsLow()
{
	SetGraphicsQuality(0);
}

void UMobaSettingsWidget::OnGraphicsMedium()
{
	SetGraphicsQuality(1);
}

void UMobaSettingsWidget::OnGraphicsHigh()
{
	SetGraphicsQuality(2);
}

void UMobaSettingsWidget::OnGraphicsEpic()
{
	SetGraphicsQuality(3);
}

void UMobaSettingsWidget::SetGraphicsQuality(int32 Level)
{
	if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
	{
		GI->SetGraphicsQuality(Level);
	}
	RefreshGraphicsButtons();
}

void UMobaSettingsWidget::RefreshGraphicsButtons()
{
	int32 Selected = 0;
	if (const UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
	{
		Selected = GI->GetGraphicsQuality();
	}
	const FLinearColor Gold(0.784f, 0.667f, 0.431f, 1.f);
	const FLinearColor LeaveGray(0.357f, 0.353f, 0.337f, 1.f);
	const FLinearColor Cream(0.941f, 0.902f, 0.824f, 1.f);
	const FLinearColor Ink(0.004f, 0.039f, 0.075f, 1.f);
	auto MakeRoundBrush = [](const FLinearColor& Color, float Radius)
	{
		FSlateBrush Brush;
		if (const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush"))
		{
			Brush = *White;
		}
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Width = 0.f;
		return Brush;
	};
	for (int32 i = 0; i < GraphicsButtons.Num(); ++i)
	{
		UButton* Btn = GraphicsButtons[i];
		if (!Btn)
		{
			continue;
		}
		const bool bOn = (i == Selected);
		const FLinearColor Color = bOn ? Gold : LeaveGray;
		FButtonStyle Style = Btn->GetStyle();
		Style.Normal = MakeRoundBrush(Color, 8.f);
		FLinearColor Hover = Color + FLinearColor(0.08f, 0.10f, 0.10f, 0.f);
		Hover.A = 1.f;
		Style.Hovered = MakeRoundBrush(Hover, 8.f);
		FLinearColor Press = Color * 0.82f;
		Press.A = 1.f;
		Style.Pressed = MakeRoundBrush(Press, 8.f);
		Style.NormalPadding = FMargin(0.f);
		Style.PressedPadding = FMargin(0.f);
		Btn->SetStyle(Style);
		if (GraphicsLabels.IsValidIndex(i) && GraphicsLabels[i])
		{
			GraphicsLabels[i]->SetColorAndOpacity(FSlateColor(bOn ? Ink : Cream));
		}
	}
}

void UMobaSettingsWidget::OnBackClicked()
{
	if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
	{
		GI->HideSettings();
	}
}

void UMobaSettingsWidget::OnMenuClicked()
{
	UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>();
	APlayerController* PC = GetOwningPlayer();
	if (GI && PC && PC->HasAuthority())
	{
		GI->ReturnToMenu();
		return;
	}
	if (GI)
	{
		GI->ShowLoadingScreen(TEXT("RETURNING TO MENU..."));
	}
	if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(GetOwningPlayerPawn()))
	{
		Hero->ServerRequestReturnToMenu();
		return;
	}
	if (GI)
	{
		GI->ReturnToMenu();
	}
}

void UMobaSettingsWidget::OnQuitClicked()
{
	if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
	{
		GI->QuitGame();
	}
}

FReply UMobaSettingsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::BackSpace || InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnBackClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
