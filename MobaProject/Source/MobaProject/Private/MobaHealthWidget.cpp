#include "MobaHealthWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/WidgetComponent.h"
#include "MobaBaseCharacter.h"

void UMobaHealthWidget::SetOwnerCharacter(AMobaBaseCharacter* InOwner)
{
	OwnerCharacter = InOwner;
}

TSharedRef<SWidget> UMobaHealthWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Bar"));
		Bar->SetFillColorAndOpacity(FLinearColor(0.2f, 0.8f, 0.25f, 1.f));
		WidgetTree->RootWidget = Bar;
	}
	return Super::RebuildWidget();
}

void UMobaHealthWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (const UWidgetComponent* Comp = GetTypedOuter<UWidgetComponent>())
	{
		OwnerCharacter = Cast<AMobaBaseCharacter>(Comp->GetOwner());
	}
}

void UMobaHealthWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!Bar || !OwnerCharacter)
	{
		return;
	}

	const float Max = OwnerCharacter->GetMaxHealth();
	Bar->SetPercent(Max > 0.f ? OwnerCharacter->GetHealth() / Max : 0.f);
}
