#include "MobaHealthWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/WidgetComponent.h"
#include "MobaAttributeSet.h"
#include "MobaBaseCharacter.h"

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
		Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Bar"));
		Bar->SetFillColorAndOpacity(FLinearColor(0.2f, 0.8f, 0.25f, 1.f));
		WidgetTree->RootWidget = Bar;
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

	if (!Bar || !OwnerActor)
	{
		return;
	}

	const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OwnerActor);
	UAbilitySystemComponent* ASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr;
	const UMobaAttributeSet* Set = ASC ? ASC->GetSet<UMobaAttributeSet>() : nullptr;
	if (!Set)
	{
		return;
	}

	const float Max = Set->GetMaxHealth();
	Bar->SetPercent(Max > 0.f ? Set->GetHealth() / Max : 0.f);
}
