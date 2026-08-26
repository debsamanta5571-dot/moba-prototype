#include "MobaMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "MobaGameInstance.h"
#include "Misc/CoreMisc.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

namespace
{
	const FLinearColor Bg(0.004f, 0.039f, 0.075f, 1.f);
	const FLinearColor CardFill(0.118f, 0.137f, 0.157f, 1.f);
	const FLinearColor Line(0.784f, 0.667f, 0.431f, 1.f);
	const FLinearColor Title(0.941f, 0.902f, 0.824f, 1.f);
	const FLinearColor Muted(0.627f, 0.608f, 0.549f, 1.f);
	const FLinearColor Ink(0.004f, 0.039f, 0.075f, 1.f);
	const FLinearColor Host(0.784f, 0.608f, 0.235f, 1.f);
	const FLinearColor Join(0.659f, 0.510f, 0.196f, 1.f);
	const FLinearColor Settings(0.471f, 0.353f, 0.157f, 1.f);
	const FLinearColor Quit(0.357f, 0.353f, 0.337f, 1.f);
	const FLinearColor Cream(0.941f, 0.902f, 0.824f, 1.f);

	FSlateBrush MakeRoundBrush(const FLinearColor& Color, float Radius)
	{
		FSlateBrush Brush;
		if (const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush"))
		{
			Brush = *White;
		}
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.Margin = FMargin(0.f);
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Width = 0.f;
		return Brush;
	}

	void StyleRoundBorder(UBorder* Border, const FLinearColor& Color, float Radius)
	{
		if (Border)
		{
			Border->SetBrush(MakeRoundBrush(Color, Radius));
		}
	}

	void StyleRoundButton(UButton* Button, const FLinearColor& Color, float Radius)
	{
		if (!Button)
		{
			return;
		}
		FLinearColor Hover = Color + FLinearColor(0.08f, 0.10f, 0.10f, 0.f);
		Hover.A = 1.f;
		FLinearColor Press = Color * 0.82f;
		Press.A = 1.f;
		FButtonStyle Style = Button->GetStyle();
		Style.Normal = MakeRoundBrush(Color, Radius);
		Style.Hovered = MakeRoundBrush(Hover, Radius);
		Style.Pressed = MakeRoundBrush(Press, Radius);
		Style.Disabled = MakeRoundBrush(FLinearColor(Color.R, Color.G, Color.B, 0.45f), Radius);
		Style.NormalPadding = FMargin(0.f);
		Style.PressedPadding = FMargin(0.f);
		Button->SetStyle(Style);
	}

	UTextBlock* MakeLabel(UWidgetTree* Tree, const FName Name, const FString& Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetText(FText::FromString(Text));
		Label->SetColorAndOpacity(FSlateColor(Color));
		Label->SetJustification(ETextJustify::Center);
		Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", Size));
		Label->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Label;
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
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));

		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
		Backdrop->SetBrushColor(Bg);
		Backdrop->SetPadding(FMargin(0.f));
		Backdrop->SetVisibility(ESlateVisibility::Visible);
		if (UOverlaySlot* Fill = Root->AddChildToOverlay(Backdrop))
		{
			Fill->SetHorizontalAlignment(HAlign_Fill);
			Fill->SetVerticalAlignment(VAlign_Fill);
		}

		USizeBox* GoldWashSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GoldWashSize"));
		GoldWashSize->SetWidthOverride(560.f);
		GoldWashSize->SetVisibility(ESlateVisibility::HitTestInvisible);
		UBorder* GoldWash = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GoldWash"));
		GoldWash->SetBrushColor(FLinearColor(0.45f, 0.32f, 0.08f, 0.28f));
		GoldWash->SetVisibility(ESlateVisibility::HitTestInvisible);
		GoldWashSize->AddChild(GoldWash);
		if (UOverlaySlot* GoldSlot = Root->AddChildToOverlay(GoldWashSize))
		{
			GoldSlot->SetHorizontalAlignment(HAlign_Left);
			GoldSlot->SetVerticalAlignment(VAlign_Fill);
		}

		USizeBox* BronzeWashSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BronzeWashSize"));
		BronzeWashSize->SetWidthOverride(560.f);
		BronzeWashSize->SetVisibility(ESlateVisibility::HitTestInvisible);
		UBorder* BronzeWash = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BronzeWash"));
		BronzeWash->SetBrushColor(FLinearColor(0.18f, 0.12f, 0.04f, 0.32f));
		BronzeWash->SetVisibility(ESlateVisibility::HitTestInvisible);
		BronzeWashSize->AddChild(BronzeWash);
		if (UOverlaySlot* BronzeSlot = Root->AddChildToOverlay(BronzeWashSize))
		{
			BronzeSlot->SetHorizontalAlignment(HAlign_Right);
			BronzeSlot->SetVerticalAlignment(VAlign_Fill);
		}

		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CardSize"));
		CardSize->SetWidthOverride(440.f);
		if (UOverlaySlot* Center = Root->AddChildToOverlay(CardSize))
		{
			Center->SetHorizontalAlignment(HAlign_Center);
			Center->SetVerticalAlignment(VAlign_Center);
		}

		UBorder* Hairline = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Hairline"));
		StyleRoundBorder(Hairline, Line, 18.f);
		Hairline->SetPadding(FMargin(2.f));
		CardSize->AddChild(Hairline);

		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Card"));
		StyleRoundBorder(Card, CardFill, 17.f);
		Card->SetPadding(FMargin(40.f, 36.f));
		Hairline->AddChild(Card);

		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
		Card->AddChild(Box);

		AddPadded(Box, MakeLabel(WidgetTree, TEXT("Title"), TEXT("MOBA PROTOTYPE"), 26, Title), 20.f);

		NoticeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Notice"));
		NoticeText->SetJustification(ETextJustify::Center);
		NoticeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.761f, 0.231f, 0.173f, 1.f)));
		NoticeText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 13));
		NoticeText->SetVisibility(ESlateVisibility::Collapsed);
		NoticeText->SetText(FText::GetEmpty());
		AddPadded(Box, NoticeText, 16.f);

		auto MakeMenuButton = [this](const FName Name, const FString& Label, const FLinearColor& Color, const FLinearColor& TextColor, float Height) -> UButton*
		{
			USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("%sSize"), *Name.ToString()));
			Size->SetHeightOverride(Height);

			UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
			StyleRoundButton(Button, Color, 10.f);
			Button->SetVisibility(ESlateVisibility::Visible);
			Button->SetIsEnabled(true);
			Button->AddChild(MakeLabel(WidgetTree, *FString::Printf(TEXT("%sLabel"), *Name.ToString()), Label, 16, TextColor));
			Size->AddChild(Button);
			return Button;
		};

		HostButton = MakeMenuButton(TEXT("Host"), TEXT("Host"), Host, Ink, 52.f);
		AddPadded(Box, HostButton->GetParent(), 20.f);

		auto StyleJoinEdit = [&](UEditableTextBox* Edit)
		{
			if (!Edit)
			{
				return;
			}
			Edit->SetIsReadOnly(false);
			Edit->SetIsPassword(false);
			Edit->SetClearKeyboardFocusOnCommit(false);
			Edit->SetSelectAllTextWhenFocused(true);
			Edit->SetForegroundColor(Title);
			Edit->SetJustification(ETextJustify::Center);
			Edit->SetVisibility(ESlateVisibility::Visible);
			FEditableTextBoxStyle EditStyle = Edit->WidgetStyle;
			EditStyle.SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
			EditStyle.ForegroundColor = FSlateColor(Title);
			EditStyle.FocusedForegroundColor = FSlateColor(Title);
			EditStyle.BackgroundColor = FSlateColor(FLinearColor::Transparent);
			EditStyle.Padding = FMargin(10.f, 0.f);
			FSlateBrush Clear;
			Clear.DrawAs = ESlateBrushDrawType::NoDrawType;
			EditStyle.BackgroundImageNormal = Clear;
			EditStyle.BackgroundImageHovered = Clear;
			EditStyle.BackgroundImageFocused = Clear;
			EditStyle.BackgroundImageReadOnly = Clear;
			Edit->WidgetStyle = EditStyle;
		};

		auto MakeJoinField = [&](const FName Name) -> UBorder*
		{
			UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("%sFrame"), *Name.ToString()));
			FSlateBrush Field = MakeRoundBrush(FLinearColor(0.02f, 0.05f, 0.09f, 1.f), 10.f);
			Field.OutlineSettings.Width = 1.5f;
			Field.OutlineSettings.Color = Line;
			Frame->SetBrush(Field);
			Frame->SetPadding(FMargin(2.f, 0.f));
			Frame->SetVisibility(ESlateVisibility::Visible);
			Frame->SetHorizontalAlignment(HAlign_Fill);
			Frame->SetVerticalAlignment(VAlign_Fill);
			return Frame;
		};

		UHorizontalBox* AddressRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("AddressRow"));

		USizeBox* AddressSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AddressSize"));
		AddressSize->SetHeightOverride(52.f);
		UBorder* AddressFrame = MakeJoinField(TEXT("Address"));
		AddressSize->AddChild(AddressFrame);
		AddressBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Address"));
		AddressBox->SetText(FText::GetEmpty());
		AddressBox->SetHintText(FText::FromString(TEXT("127.0.0.1")));
		StyleJoinEdit(AddressBox);
		AddressFrame->AddChild(AddressBox);

		USizeBox* PortSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PortSize"));
		PortSize->SetHeightOverride(52.f);
		PortSize->SetWidthOverride(112.f);
		UBorder* PortFrame = MakeJoinField(TEXT("Port"));
		PortSize->AddChild(PortFrame);
		PortBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Port"));
		PortBox->SetText(FText::FromString(TEXT("7777")));
		PortBox->SetHintText(FText::FromString(TEXT("7777")));
		StyleJoinEdit(PortBox);
		PortFrame->AddChild(PortBox);

		if (UHorizontalBoxSlot* IpSlot = AddressRow->AddChildToHorizontalBox(AddressSize))
		{
			IpSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			IpSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
			IpSlot->SetVerticalAlignment(VAlign_Fill);
		}
		if (UHorizontalBoxSlot* PortSlot = AddressRow->AddChildToHorizontalBox(PortSize))
		{
			PortSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			PortSlot->SetVerticalAlignment(VAlign_Fill);
		}
		AddPadded(Box, AddressRow, 12.f);

		JoinButton = MakeMenuButton(TEXT("Join"), TEXT("Join"), Join, Ink, 48.f);
		AddPadded(Box, JoinButton->GetParent(), 10.f);

		SettingsButton = MakeMenuButton(TEXT("Settings"), TEXT("Settings"), Settings, Cream, 44.f);
		AddPadded(Box, SettingsButton->GetParent(), 10.f);

		QuitButton = MakeMenuButton(TEXT("Quit"), TEXT("Quit"), Quit, Cream, 44.f);
		Box->AddChildToVerticalBox(QuitButton->GetParent());

		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

void UMobaMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance()))
	{
		GI->RestoreUiPointerIfNeeded();
	}
}

void UMobaMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
	SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
	SetAlignmentInViewport(FVector2D(0.f, 0.f));

	if (HostButton)
	{
		HostButton->OnClicked.AddUniqueDynamic(this, &UMobaMenuWidget::OnHostClicked);
		if (IsRunningClientOnly())
		{
			HostButton->SetIsEnabled(false);
			if (UWidget* HostRow = HostButton->GetParent())
			{
				HostRow->SetVisibility(ESlateVisibility::Collapsed);
			}
			HostButton->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddUniqueDynamic(this, &UMobaMenuWidget::OnJoinClicked);
	}
	if (AddressBox)
	{
		AddressBox->OnTextCommitted.AddUniqueDynamic(this, &UMobaMenuWidget::OnJoinAddressCommitted);
	}
	if (PortBox)
	{
		PortBox->OnTextCommitted.AddUniqueDynamic(this, &UMobaMenuWidget::OnJoinAddressCommitted);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddUniqueDynamic(this, &UMobaMenuWidget::OnSettingsClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddUniqueDynamic(this, &UMobaMenuWidget::OnQuitClicked);
	}
}

void UMobaMenuWidget::SetNotice(const FString& Message)
{
	if (!NoticeText)
	{
		return;
	}
	const bool bShow = !Message.IsEmpty();
	NoticeText->SetText(FText::FromString(Message));
	NoticeText->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UMobaMenuWidget::OnHostClicked()
{
	if (IsRunningClientOnly())
	{
		return;
	}
	if (UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance()))
	{
		GI->HostGame();
	}
}

FString UMobaMenuWidget::GetJoinHost() const
{
	FString JoinHost;
	if (AddressBox)
	{
		JoinHost = AddressBox->GetText().ToString().TrimStartAndEnd();
	}
	int32 Colon = INDEX_NONE;
	if (JoinHost.FindLastChar(TEXT(':'), Colon) && Colon > 0)
	{
		JoinHost.LeftInline(Colon);
	}
	if (JoinHost.IsEmpty())
	{
		return TEXT("127.0.0.1");
	}
	return JoinHost;
}

int32 UMobaMenuWidget::GetJoinPort() const
{
	FString PortText;
	if (PortBox)
	{
		PortText = PortBox->GetText().ToString().TrimStartAndEnd();
	}
	if (!PortText.IsEmpty() && PortText.IsNumeric())
	{
		return FMath::Clamp(FCString::Atoi(*PortText), 1, 65535);
	}
	if (AddressBox)
	{
		const FString Typed = AddressBox->GetText().ToString().TrimStartAndEnd();
		int32 Colon = INDEX_NONE;
		if (Typed.FindLastChar(TEXT(':'), Colon) && Colon > 0)
		{
			const FString Tail = Typed.Mid(Colon + 1);
			if (Tail.IsNumeric())
			{
				return FMath::Clamp(FCString::Atoi(*Tail), 1, 65535);
			}
		}
	}
	return 7777;
}

FString UMobaMenuWidget::GetJoinAddress() const
{
	const FString JoinHost = GetJoinHost();
	if (JoinHost.IsEmpty())
	{
		return FString();
	}
	return FString::Printf(TEXT("%s:%d"), *JoinHost, GetJoinPort());
}

void UMobaMenuWidget::FocusJoinAddress()
{
	if (AddressBox)
	{
		AddressBox->SetKeyboardFocus();
	}
}

void UMobaMenuWidget::OnJoinClicked()
{
	UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}
	GI->JoinGame(GetJoinAddress());
}

void UMobaMenuWidget::OnJoinAddressCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	(void)Text;
	if (CommitMethod == ETextCommit::OnEnter)
	{
		OnJoinClicked();
	}
}

void UMobaMenuWidget::OnSettingsClicked()
{
	if (UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance()))
	{
		GI->ShowSettings();
	}
}

void UMobaMenuWidget::OnQuitClicked()
{
	if (UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance()))
	{
		GI->QuitGame();
	}
}
