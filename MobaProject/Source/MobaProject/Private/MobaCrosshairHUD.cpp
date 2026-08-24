#include "MobaCrosshairHUD.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Engine/LocalPlayer.h"

namespace
{
	USizeBox* MakeCrossBar(UWidgetTree* Tree, const FName Name, float Width, float Height, const FLinearColor& Color)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), Name);
		Box->SetWidthOverride(Width);
		Box->SetHeightOverride(Height);
		UBorder* Fill = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("%sFill"), *Name.ToString()));
		Fill->SetBrushColor(Color);
		Fill->SetPadding(FMargin(0.f));
		Box->AddChild(Fill);
		return Box;
	}
}

void UMobaCrosshairHUD::PlaceInViewport()
{
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::HitTestInvisible);

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

TSharedRef<SWidget> UMobaCrosshairHUD::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		const FLinearColor Color(1.f, 1.f, 1.f, 0.95f);

		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		Root->SetVisibility(ESlateVisibility::HitTestInvisible);

		if (UOverlaySlot* HSlot = Root->AddChildToOverlay(MakeCrossBar(WidgetTree, TEXT("Horiz"), 22.f, 2.f, Color)))
		{
			HSlot->SetHorizontalAlignment(HAlign_Center);
			HSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UOverlaySlot* VSlot = Root->AddChildToOverlay(MakeCrossBar(WidgetTree, TEXT("Vert"), 2.f, 22.f, Color)))
		{
			VSlot->SetHorizontalAlignment(HAlign_Center);
			VSlot->SetVerticalAlignment(VAlign_Center);
		}

		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

void UMobaCrosshairHUD::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}
