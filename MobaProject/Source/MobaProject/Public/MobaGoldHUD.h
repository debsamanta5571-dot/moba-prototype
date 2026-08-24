#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "MobaGoldHUD.generated.h"

class AMobaBaseCharacter;
class UBorder;
class UTextBlock;

UCLASS()
class MOBAPROJECT_API UMobaGoldHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UMobaGoldHUD(const FObjectInitializer& ObjectInitializer);

	void SetOwnerCharacter(AMobaBaseCharacter* InOwner);
	void PlaceInViewport();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void Rebuild();
	void UpdateGold();

	UPROPERTY()
	TObjectPtr<AMobaBaseCharacter> OwnerCharacter;

	UPROPERTY()
	TObjectPtr<UTextBlock> GoldText;

	UPROPERTY()
	TObjectPtr<UBorder> BuyKeyFrame;

	UPROPERTY()
	TObjectPtr<UTextBlock> BuyKeyText;
};
