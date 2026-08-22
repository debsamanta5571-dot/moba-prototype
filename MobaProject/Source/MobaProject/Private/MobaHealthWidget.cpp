#include "MobaHealthWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "MobaAttributeSet.h"
#include "MobaBaseCharacter.h"
#include "MobaMinion.h"
#include "Styling/CoreStyle.h"

namespace
{
	AActor* MobaLocalViewer(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		if (!World)
		{
			return nullptr;
		}
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC || !PC->IsLocalController())
			{
				continue;
			}
			if (APawn* Pawn = PC->GetPawn())
			{
				return Pawn;
			}
			return PC->PlayerState.Get();
		}
		return nullptr;
	}
}

void UMobaHealthWidget::SetOwnerActor(AActor* InOwner)
{
	OwnerActor = InOwner;
}

void UMobaHealthWidget::SetOwnerCharacter(AMobaBaseCharacter* InOwner)
{
	OwnerActor = InOwner;
}

TSharedRef<SWidget> UMobaHealthWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Bar"));
		Bar->SetFillColorAndOpacity(FLinearColor(0.2f, 0.8f, 0.25f, 1.f));
		if (UOverlaySlot* BarSlot = Root->AddChildToOverlay(Bar))
		{
			BarSlot->SetHorizontalAlignment(HAlign_Fill);
			BarSlot->SetVerticalAlignment(VAlign_Bottom);
		}

		StatusFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatusFrame"));
		StatusFrame->SetBrushColor(FLinearColor(0.08f, 0.03f, 0.14f, 0.94f));
		StatusFrame->SetPadding(FMargin(6.f, 2.f));
		StatusFrame->SetVisibility(ESlateVisibility::Collapsed);

		StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
		StatusText->SetJustification(ETextJustify::Center);
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.72f, 1.f, 1.f)));
		StatusText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 11));
		StatusText->SetText(FText::GetEmpty());
		StatusFrame->AddChild(StatusText);
		if (UOverlaySlot* StatusSlot = Root->AddChildToOverlay(StatusFrame))
		{
			StatusSlot->SetHorizontalAlignment(HAlign_Center);
			StatusSlot->SetVerticalAlignment(VAlign_Top);
		}
	}
	return Super::RebuildWidget();
}

void UMobaHealthWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!OwnerActor)
	{
		if (const UWidgetComponent* Comp = GetTypedOuter<UWidgetComponent>())
		{
			OwnerActor = Comp->GetOwner();
		}
	}
}

void UMobaHealthWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!OwnerActor)
	{
		return;
	}

	UpdateStatus();

	if (!Bar)
	{
		return;
	}

	const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OwnerActor);
	UAbilitySystemComponent* ASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr;
	const UMobaAttributeSet* Set = ASC ? ASC->GetSet<UMobaAttributeSet>() : nullptr;
	Bar->SetFillColorAndOpacity(MobaAttitudeColor(MobaLocalViewer(this), OwnerActor));

	if (Set)
	{
		const float Max = Set->GetMaxHealth();
		Bar->SetPercent(Max > 0.f ? Set->GetHealth() / Max : 0.f);
		return;
	}

	if (const AMobaMinion* Minion = Cast<AMobaMinion>(OwnerActor))
	{
		const float Health = Minion->GetHealth();
		Bar->SetPercent(Health > 0.f ? 1.f : 0.f);
	}
}

void UMobaHealthWidget::UpdateStatus()
{
	if (!OwnerActor)
	{
		return;
	}

	bool bStunned = false;
	bool bSlowed = false;
	if (const AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(OwnerActor))
	{
		bStunned = Hero->IsStunned();
		bSlowed = Hero->IsSlowed();
	}
	else if (const AMobaMinion* Minion = Cast<AMobaMinion>(OwnerActor))
	{
		bStunned = Minion->IsStunned();
		bSlowed = Minion->IsSlowed();
	}

	if (!StatusFrame || !StatusText)
	{
		return;
	}

	if (bStunned)
	{
		StatusText->SetText(FText::FromString(TEXT("Stunned")));
		StatusFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else if (bSlowed)
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
