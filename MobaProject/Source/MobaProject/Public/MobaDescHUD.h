#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "MobaDescHUD.generated.h"

class UMobaDescComponent;
class UTextBlock;
class UVerticalBox;

UCLASS()
class MOBAPROJECT_API UMobaDescHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UMobaDescHUD(const FObjectInitializer& ObjectInitializer);

	void SetDescComponent(UMobaDescComponent* InDesc);
	void PlaceInViewport();
	void RebuildList();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	float LastDamageBoost = -1.f;

	UPROPERTY()
	TObjectPtr<UMobaDescComponent> DescComponent;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> ListBox;
};
