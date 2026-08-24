#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MobaDamageNumber.generated.h"

class USceneComponent;
class UTextBlock;
class UWidgetComponent;

UCLASS()
class MOBAPROJECT_API UMobaDamageNumberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetAmount(float Amount, bool bInGold = false);
	void SetFade(float Alpha);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	void ApplyAmount();
	FLinearColor GetLabelColor(float Alpha) const;

	UPROPERTY()
	TObjectPtr<UTextBlock> Label;

	float StoredAmount = 0.f;
	bool bGold = false;
};

UCLASS()
class MOBAPROJECT_API AMobaDamageNumber : public AActor
{
	GENERATED_BODY()

public:
	AMobaDamageNumber();

	void Init(float Amount, bool bInGold = false);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Moba")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Moba")
	TObjectPtr<UWidgetComponent> Widget;

	void ApplyToWidget();

	float Age = 0.f;
	float Lifetime = 0.9f;
	float RiseSpeed = 90.f;
	float PendingAmount = 0.f;
	bool bGold = false;
};
