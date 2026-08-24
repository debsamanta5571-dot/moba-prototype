#include "MobaDamageNumber.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SceneComponent.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Styling/CoreStyle.h"

void UMobaDamageNumberWidget::SetAmount(float Amount, bool bInGold)
{
	StoredAmount = Amount;
	bGold = bInGold;
	ApplyAmount();
}

FLinearColor UMobaDamageNumberWidget::GetLabelColor(float Alpha) const
{
	return bGold
		? FLinearColor(1.f, 0.84f, 0.12f, Alpha)
		: FLinearColor(1.f, 0.22f, 0.18f, Alpha);
}

void UMobaDamageNumberWidget::ApplyAmount()
{
	if (!Label)
	{
		return;
	}
	const int32 Value = FMath::RoundToInt(StoredAmount);
	Label->SetText(FText::FromString(bGold
		? FString::Printf(TEXT("+%d"), Value)
		: FString::Printf(TEXT("%d"), Value)));
	Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", bGold ? 26 : 22));
	Label->SetColorAndOpacity(FSlateColor(GetLabelColor(1.f)));
}

void UMobaDamageNumberWidget::SetFade(float Alpha)
{
	SetRenderOpacity(Alpha);
	if (Label)
	{
		Label->SetColorAndOpacity(FSlateColor(GetLabelColor(Alpha)));
	}
}

TSharedRef<SWidget> UMobaDamageNumberWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
		Label->SetJustification(ETextJustify::Center);
		Label->SetColorAndOpacity(FSlateColor(GetLabelColor(1.f)));
		Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", bGold ? 26 : 22));
		Label->SetText(FText::FromString(bGold
			? FString::Printf(TEXT("+%d"), FMath::RoundToInt(StoredAmount))
			: FString::Printf(TEXT("%d"), FMath::RoundToInt(StoredAmount))));
		WidgetTree->RootWidget = Label;
	}
	return Super::RebuildWidget();
}

void UMobaDamageNumberWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyAmount();
}

AMobaDamageNumber::AMobaDamageNumber()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetCanBeDamaged(false);
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Widget = CreateDefaultSubobject<UWidgetComponent>(TEXT("Widget"));
	Widget->SetupAttachment(SceneRoot);
	Widget->SetWidgetSpace(EWidgetSpace::Screen);
	Widget->SetDrawSize(FVector2D(180.f, 52.f));
	Widget->SetPivot(FVector2D(0.5f, 0.5f));
	Widget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Widget->SetTickWhenOffscreen(true);
	Widget->SetTwoSided(true);
	Widget->SetWidgetClass(UMobaDamageNumberWidget::StaticClass());
}

void AMobaDamageNumber::BeginPlay()
{
	Super::BeginPlay();
	if (Widget)
	{
		Widget->InitWidget();
	}
}

void AMobaDamageNumber::Init(float Amount, bool bInGold)
{
	PendingAmount = Amount;
	bGold = bInGold;
	if (bGold)
	{
		Lifetime = 1.15f;
		RiseSpeed = 110.f;
	}
	if (Widget)
	{
		Widget->InitWidget();
	}
	ApplyToWidget();
	SetLifeSpan(Lifetime);
}

void AMobaDamageNumber::ApplyToWidget()
{
	if (!Widget)
	{
		return;
	}
	if (!Widget->GetWidget())
	{
		Widget->InitWidget();
	}
	if (UMobaDamageNumberWidget* UI = Cast<UMobaDamageNumberWidget>(Widget->GetWidget()))
	{
		UI->SetAmount(PendingAmount, bGold);
	}
}

void AMobaDamageNumber::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	AddActorWorldOffset(FVector(0.f, 0.f, RiseSpeed * DeltaSeconds));

	const float FadeStart = Lifetime * 0.45f;
	const float Alpha = Age < FadeStart
		? 1.f
		: FMath::Clamp(1.f - (Age - FadeStart) / FMath::Max(Lifetime - FadeStart, 0.01f), 0.f, 1.f);

	if (UMobaDamageNumberWidget* UI = Widget ? Cast<UMobaDamageNumberWidget>(Widget->GetWidget()) : nullptr)
	{
		UI->SetFade(Alpha);
	}
}
