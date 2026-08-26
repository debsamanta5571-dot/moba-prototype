#include "MobaLobbyWidget.h"
#include "AMobaPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Slider.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "MobaGameInstance.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "TimerManager.h"

namespace
{
	const FLinearColor Cream(0.941f, 0.902f, 0.824f, 1.f);
	const FLinearColor Muted(0.627f, 0.608f, 0.549f, 1.f);
	const FLinearColor Gold(0.784f, 0.667f, 0.431f, 1.f);
	const FLinearColor Ink(0.004f, 0.039f, 0.075f, 1.f);
	const FLinearColor Team1Color(0.10f, 0.38f, 0.62f, 1.f);
	const FLinearColor Team1Selected(0.16f, 0.52f, 0.82f, 1.f);
	const FLinearColor Team2Color(0.761f, 0.231f, 0.173f, 1.f);
	const FLinearColor Team2Selected(0.910f, 0.251f, 0.341f, 1.f);
	const FLinearColor HostGreen(0.784f, 0.608f, 0.235f, 1.f);
	const FLinearColor LeaveGray(0.357f, 0.353f, 0.337f, 1.f);

	FSlateBrush MakeRoundBrush(const FLinearColor& Color, float Radius)
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

	UButton* MakeColorButton(UWidgetTree* Tree, const FName Name, const FString& Label, const FLinearColor& Color, float Height, const FLinearColor& TextColor)
	{
		USizeBox* Size = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("%sSize"), *Name.ToString()));
		Size->SetHeightOverride(Height);
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		StyleRoundButton(Button, Color, 10.f);
		Button->AddChild(MakeLabel(Tree, *FString::Printf(TEXT("%sLabel"), *Name.ToString()), Label, 16, TextColor));
		Size->AddChild(Button);
		return Button;
	}
}

void UMobaLobbyWidget::PlaceInViewport()
{
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
	if (!IsInViewport())
	{
		AddToViewport(110);
	}
	SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
	SetAlignmentInViewport(FVector2D(0.f, 0.f));
	LastListSignature.Reset();
	Refresh();
}

void UMobaLobbyWidget::SetJoinInProgress(bool bJoin)
{
	bJoinInProgress = bJoin;
	LastListSignature.Reset();
	Refresh();
}

TSharedRef<SWidget> UMobaLobbyWidget::RebuildWidget()
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

		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CardSize"));
		CardSize->SetWidthOverride(460.f);
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
		Card->SetPadding(FMargin(36.f, 32.f));
		Hairline->AddChild(Card);

		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
		Card->AddChild(Box);

		TitleText = MakeLabel(WidgetTree, TEXT("Title"), TEXT("LOBBY"), 32, Gold);
		if (UVerticalBoxSlot* TitleSlot = Box->AddChildToVerticalBox(TitleText))
		{
			TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
			TitleSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		PlayerList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlayerList"));
		if (UVerticalBoxSlot* ListSlot = Box->AddChildToVerticalBox(PlayerList))
		{
			ListSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
			ListSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		if (UVerticalBoxSlot* HeroHintSlot = Box->AddChildToVerticalBox(MakeLabel(WidgetTree, TEXT("HeroHint"), TEXT("Character"), 12, Muted)))
		{
			HeroHintSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
			HeroHintSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		UHorizontalBox* HeroRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeroRow"));
		auto MakeHeroButton = [this](const FName Name, const FString& Caption, TObjectPtr<UTextBlock>& OutLabel) -> UButton*
		{
			USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("%sSize"), *Name.ToString()));
			Size->SetHeightOverride(48.f);
			UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
			StyleRoundButton(Button, LeaveGray, 10.f);
			OutLabel = MakeLabel(WidgetTree, *FString::Printf(TEXT("%sLabel"), *Name.ToString()), Caption, 16, Cream);
			Button->AddChild(OutLabel);
			Size->AddChild(Button);
			return Button;
		};
		BrawlerButton = MakeHeroButton(TEXT("Brawler"), TEXT("Brawler"), BrawlerLabel);
		MageButton = MakeHeroButton(TEXT("Mage"), TEXT("Mage"), MageLabel);
		if (UHorizontalBoxSlot* Left = HeroRow->AddChildToHorizontalBox(BrawlerButton->GetParent()))
		{
			Left->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Left->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		if (UHorizontalBoxSlot* Right = HeroRow->AddChildToHorizontalBox(MageButton->GetParent()))
		{
			Right->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Right->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}
		if (UVerticalBoxSlot* HeroSlot = Box->AddChildToVerticalBox(HeroRow))
		{
			HeroSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
			HeroSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		if (UVerticalBoxSlot* TeamHintSlot = Box->AddChildToVerticalBox(MakeLabel(WidgetTree, TEXT("TeamHint"), TEXT("Team"), 12, Muted)))
		{
			TeamHintSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
			TeamHintSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		UHorizontalBox* TeamRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TeamRow"));
		Team1Button = MakeColorButton(WidgetTree, TEXT("Team1"), TEXT("Team 1"), Team1Color, 48.f, Cream);
		Team2Button = MakeColorButton(WidgetTree, TEXT("Team2"), TEXT("Team 2"), Team2Color, 48.f, Cream);
		if (UHorizontalBoxSlot* T1 = TeamRow->AddChildToHorizontalBox(Team1Button->GetParent()))
		{
			T1->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			T1->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		if (UHorizontalBoxSlot* T2 = TeamRow->AddChildToHorizontalBox(Team2Button->GetParent()))
		{
			T2->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			T2->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}
		if (UVerticalBoxSlot* TeamSlot = Box->AddChildToVerticalBox(TeamRow))
		{
			TeamSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
			TeamSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		PingPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PingPanel"));
		if (UVerticalBoxSlot* PingPanelSlot = Box->AddChildToVerticalBox(PingPanel))
		{
			PingPanelSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
			PingPanelSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		UHorizontalBox* PingHeader = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PingHeader"));
		if (UHorizontalBoxSlot* HintSlot = PingHeader->AddChildToHorizontalBox(MakeLabel(WidgetTree, TEXT("PingHint"), TEXT("Simulated ping"), 12, Muted)))
		{
			HintSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			HintSlot->SetVerticalAlignment(VAlign_Center);
			HintSlot->SetHorizontalAlignment(HAlign_Left);
		}
		PingValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PingValue"));
		PingValueText->SetText(FText::FromString(TEXT("0 - 0 ms")));
		PingValueText->SetColorAndOpacity(FSlateColor(Cream));
		PingValueText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
		PingValueText->SetJustification(ETextJustify::Right);
		PingValueText->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UHorizontalBoxSlot* ValueSlot = PingHeader->AddChildToHorizontalBox(PingValueText))
		{
			ValueSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* PingHeaderSlot = PingPanel->AddChildToVerticalBox(PingHeader))
		{
			PingHeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
			PingHeaderSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		auto MakePingSlider = [this](const FName Name) -> USlider*
		{
			USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("%sSize"), *Name.ToString()));
			Size->SetHeightOverride(24.f);
			USlider* Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), Name);
			Slider->SetMinValue(0.f);
			Slider->SetMaxValue(300.f);
			Slider->SetStepSize(10.f);
			Slider->SetValue(0.f);
			Slider->SetSliderBarColor(FLinearColor(0.784f, 0.667f, 0.431f, 1.f));
			Slider->SetSliderHandleColor(Cream);
			Size->AddChild(Slider);
			return Slider;
		};

		auto AddSliderRow = [&](const FName Name, const FString& Caption, TObjectPtr<USlider>& OutSlider)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("%sRow"), *Name.ToString()));
			if (UHorizontalBoxSlot* CapSlot = Row->AddChildToHorizontalBox(MakeLabel(WidgetTree, *FString::Printf(TEXT("%sCap"), *Name.ToString()), Caption, 11, Muted)))
			{
				CapSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
				CapSlot->SetVerticalAlignment(VAlign_Center);
			}
			OutSlider = MakePingSlider(Name);
			if (UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(OutSlider->GetParent()))
			{
				SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				SliderSlot->SetVerticalAlignment(VAlign_Center);
			}
			if (UVerticalBoxSlot* RowSlot = PingPanel->AddChildToVerticalBox(Row))
			{
				RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
				RowSlot->SetHorizontalAlignment(HAlign_Fill);
			}
		};

		AddSliderRow(TEXT("PingMin"), TEXT("Min"), PingMinSlider);
		AddSliderRow(TEXT("PingMax"), TEXT("Max"), PingMaxSlider);

		StatusText = MakeLabel(WidgetTree, TEXT("Status"), TEXT(""), 13, Muted);
		StatusText->SetVisibility(ESlateVisibility::Collapsed);

		StartButton = MakeColorButton(WidgetTree, TEXT("Start"), TEXT("Start Game"), HostGreen, 52.f, Ink);
		if (StartButton && StartButton->GetChildrenCount() > 0)
		{
			StartLabel = Cast<UTextBlock>(StartButton->GetChildAt(0));
		}
		if (UVerticalBoxSlot* StartSlot = Box->AddChildToVerticalBox(StartButton->GetParent()))
		{
			StartSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
			StartSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		LeaveButton = MakeColorButton(WidgetTree, TEXT("Leave"), TEXT("Leave"), LeaveGray, 48.f, Cream);
		Box->AddChildToVerticalBox(LeaveButton->GetParent());

		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

void UMobaLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	if (Team1Button)
	{
		Team1Button->OnClicked.AddUniqueDynamic(this, &UMobaLobbyWidget::OnTeam1Clicked);
	}
	if (Team2Button)
	{
		Team2Button->OnClicked.AddUniqueDynamic(this, &UMobaLobbyWidget::OnTeam2Clicked);
	}
	if (BrawlerButton)
	{
		BrawlerButton->OnClicked.AddUniqueDynamic(this, &UMobaLobbyWidget::OnBrawlerClicked);
	}
	if (MageButton)
	{
		MageButton->OnClicked.AddUniqueDynamic(this, &UMobaLobbyWidget::OnMageClicked);
	}
	if (StartButton)
	{
		StartButton->OnClicked.AddUniqueDynamic(this, &UMobaLobbyWidget::OnStartClicked);
	}
	if (LeaveButton)
	{
		LeaveButton->OnClicked.AddUniqueDynamic(this, &UMobaLobbyWidget::OnLeaveClicked);
	}
	int32 PingMin = 0;
	int32 PingMax = 0;
	if (const UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance()))
	{
		PingMin = GI->GetSimulatedPingMin();
		PingMax = GI->GetSimulatedPingMax();
	}
	if (PingMinSlider)
	{
		PingMinSlider->OnValueChanged.AddUniqueDynamic(this, &UMobaLobbyWidget::OnPingMinChanged);
		PingMinSlider->SetValue(static_cast<float>(PingMin));
	}
	if (PingMaxSlider)
	{
		PingMaxSlider->OnValueChanged.AddUniqueDynamic(this, &UMobaLobbyWidget::OnPingMaxChanged);
		PingMaxSlider->SetValue(static_cast<float>(PingMax));
	}
	UpdatePingLabel();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimer, this, &UMobaLobbyWidget::Refresh, 0.25f, true);
	}
	Refresh();
}

void UMobaLobbyWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::NativeDestruct();
}

void UMobaLobbyWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Refresh();
	if (UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance()))
	{
		GI->RestoreUiPointerIfNeeded();
	}
}

void UMobaLobbyWidget::OnTeam1Clicked()
{
	RequestTeam(1);
}

void UMobaLobbyWidget::OnTeam2Clicked()
{
	RequestTeam(2);
}

void UMobaLobbyWidget::OnBrawlerClicked()
{
	if (UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance()))
	{
		GI->SetSelectedHeroIndex(0);
	}
	RefreshHeroButtons();
}

void UMobaLobbyWidget::OnMageClicked()
{
	if (UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance()))
	{
		GI->SetSelectedHeroIndex(1);
	}
	RefreshHeroButtons();
}

void UMobaLobbyWidget::OnStartClicked()
{
	if (UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance()))
	{
		if (bJoinInProgress || GI->IsJoinLoadout())
		{
			GI->ConfirmJoinLoadout();
		}
		else
		{
			GI->StartMatchFromLobby();
		}
	}
}

void UMobaLobbyWidget::OnLeaveClicked()
{
	if (UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance()))
	{
		GI->LeaveLobby();
	}
}

void UMobaLobbyWidget::OnPingMinChanged(float Value)
{
	if (PingMaxSlider && Value > PingMaxSlider->GetValue())
	{
		PingMaxSlider->SetValue(Value);
	}
	ApplyPingSliders();
}

void UMobaLobbyWidget::OnPingMaxChanged(float Value)
{
	if (PingMinSlider && Value < PingMinSlider->GetValue())
	{
		PingMinSlider->SetValue(Value);
	}
	ApplyPingSliders();
}

void UMobaLobbyWidget::ApplyPingSliders()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_Client)
	{
		return;
	}
	const int32 MinMs = PingMinSlider ? FMath::RoundToInt(PingMinSlider->GetValue()) : 0;
	const int32 MaxMs = PingMaxSlider ? FMath::RoundToInt(PingMaxSlider->GetValue()) : 0;
	UpdatePingLabel();
	if (UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance()))
	{
		GI->SetSimulatedPingRange(MinMs, MaxMs);
	}
}

void UMobaLobbyWidget::UpdatePingLabel()
{
	if (!PingValueText)
	{
		return;
	}
	const int32 MinMs = PingMinSlider ? FMath::RoundToInt(PingMinSlider->GetValue()) : 0;
	const int32 MaxMs = PingMaxSlider ? FMath::RoundToInt(PingMaxSlider->GetValue()) : 0;

	int32 LiveMs = 0;
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UNetConnection* Conn = PC->GetNetConnection())
		{
			if (Conn->AvgLag > 0.f)
			{
				LiveMs = FMath::RoundToInt(Conn->AvgLag * 1000.f);
			}
			else if (Conn->RawPingInSeconds > 0.0)
			{
				LiveMs = FMath::RoundToInt(static_cast<float>(Conn->RawPingInSeconds * 1000.0));
			}
		}
		if (LiveMs <= 0 && PC->PlayerState)
		{
			LiveMs = FMath::RoundToInt(PC->PlayerState->ExactPing);
		}
	}

	if (LiveMs > 0)
	{
		PingValueText->SetText(FText::FromString(FString::Printf(TEXT("%d - %d ms   live %d ms"), MinMs, MaxMs, LiveMs)));
	}
	else
	{
		PingValueText->SetText(FText::FromString(FString::Printf(TEXT("%d - %d ms"), MinMs, MaxMs)));
	}
}

void UMobaLobbyWidget::RequestTeam(int32 Team)
{
	if (UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance()))
	{
		GI->RequestLobbyTeam(Team);
	}
	RefreshTeamButtons();
}

void UMobaLobbyWidget::RefreshPingPanel()
{
	if (PingPanel)
	{
		PingPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMobaLobbyWidget::Refresh()
{
	RefreshPlayerList();
	RefreshTeamButtons();
	RefreshHeroButtons();
	RefreshPingPanel();
	UpdatePingLabel();

	UWorld* World = GetWorld();
	if (const UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance()))
	{
		if (GI->IsJoinLoadout())
		{
			bJoinInProgress = true;
		}
	}
	bool bLeader = World && World->GetNetMode() == NM_ListenServer;
	if (const UMobaGameInstance* LeaderGI = Cast<UMobaGameInstance>(GetGameInstance()))
	{
		bLeader = LeaderGI->IsLocalLobbyLeader();
	}
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(bJoinInProgress ? TEXT("JOIN MATCH") : TEXT("LOBBY")));
	}
	if (StartLabel)
	{
		StartLabel->SetText(FText::FromString(bJoinInProgress ? TEXT("Enter Match") : TEXT("Start Game")));
	}
	if (StartButton)
	{
		if (UWidget* StartParent = StartButton->GetParent())
		{
			StartParent->SetVisibility((bLeader || bJoinInProgress) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); // dedicated: first joiner is leader
		}
	}
	if (StatusText)
	{
		StatusText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMobaLobbyWidget::RefreshPlayerList()
{
	if (!PlayerList)
	{
		return;
	}

	UWorld* World = GetWorld();
	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	APlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<APlayerState>() : nullptr;

	FString Signature;
	if (GS)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (!PS)
			{
				continue;
			}
			const AMobaPlayerState* MobaPS = Cast<AMobaPlayerState>(PS);
			const int32 Team = MobaPS ? MobaPS->TeamID : 0;
			const int32 Hero = MobaPS ? MobaPS->HeroIndex : 0;
			Signature += PS->GetPlayerName() + TEXT(":") + FString::FromInt(Team) + TEXT(":") + FString::FromInt(Hero) + TEXT(";");
		}
	}
	if (Signature == LastListSignature)
	{
		return;
	}
	LastListSignature = Signature;
	PlayerList->ClearChildren();

	if (!GS || GS->PlayerArray.Num() == 0)
	{
		UTextBlock* Empty = MakeLabel(WidgetTree, TEXT("EmptyPlayers"), TEXT("Waiting for players..."), 14, Muted);
		PlayerList->AddChildToVerticalBox(Empty);
		return;
	}

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS)
		{
			continue;
		}
		const AMobaPlayerState* MobaPS = Cast<AMobaPlayerState>(PS);
		const int32 Team = MobaPS ? MobaPS->TeamID : 0;
		const int32 Hero = MobaPS ? MobaPS->HeroIndex : 0;
		FString Name = PS->GetPlayerName();
		if (!Name.StartsWith(TEXT("Player ")))
		{
			Name = FString::Printf(TEXT("Player %d"), PS->GetPlayerId() + 1);
		}
		FString TeamName = TEXT("No team");
		if (Team == 1)
		{
			TeamName = TEXT("Team 1");
		}
		else if (Team == 2)
		{
			TeamName = TEXT("Team 2");
		}
		const FString HeroName = (Hero == 1) ? TEXT("Mage") : TEXT("Brawler");
		FString Line = Name + TEXT("   ") + HeroName + TEXT("   ") + TeamName;
		if (PS == LocalPS)
		{
			Line += TEXT("  (You)");
		}

		UTextBlock* Label = MakeLabel(WidgetTree, NAME_None, Line, 15, Team == 2 ? Team2Selected : (Team == 1 ? Team1Selected : Cream));
		Label->SetJustification(ETextJustify::Left);
		if (UVerticalBoxSlot* ListSlot = PlayerList->AddChildToVerticalBox(Label))
		{
			ListSlot->SetPadding(FMargin(8.f, 3.f));
			ListSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

void UMobaLobbyWidget::RefreshHeroButtons()
{
	int32 Selected = 0;
	if (const UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance()))
	{
		Selected = GI->GetSelectedHeroIndex();
	}
	auto Paint = [](UButton* Button, UTextBlock* Label, bool bSelected)
	{
		if (Button)
		{
			StyleRoundButton(Button, bSelected ? Gold : LeaveGray, 10.f);
		}
		if (Label)
		{
			Label->SetColorAndOpacity(FSlateColor(bSelected ? Ink : Cream));
		}
	};
	Paint(BrawlerButton, BrawlerLabel, Selected == 0);
	Paint(MageButton, MageLabel, Selected == 1);
}

void UMobaLobbyWidget::RefreshTeamButtons()
{
	int32 LocalTeam = 0;
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (const AMobaPlayerState* PS = PC->GetPlayerState<AMobaPlayerState>())
		{
			LocalTeam = PS->TeamID;
		}
	}
	if (LocalTeam != 1 && LocalTeam != 2)
	{
		if (const UMobaGameInstance* GI = Cast<UMobaGameInstance>(GetGameInstance()))
		{
			LocalTeam = GI->GetPendingTeamId();
		}
	}
	if (Team1Button)
	{
		StyleRoundButton(Team1Button, LocalTeam == 1 ? Team1Selected : Team1Color, 10.f);
	}
	if (Team2Button)
	{
		StyleRoundButton(Team2Button, LocalTeam == 2 ? Team2Selected : Team2Color, 10.f);
	}
}
