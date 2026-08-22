#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "CoreMinimal.h"
#include "MobaShopHUD.generated.h"

class AMobaBaseCharacter;
class UTextBlock;
class UMobaShopHUD;

UCLASS()
class MOBAPROJECT_API UMobaShopBuyButton : public UButton
{
	GENERATED_BODY()

public:
	int32 OfferIndex = 0;
	TWeakObjectPtr<UMobaShopHUD> ShopHUD;

	UFUNCTION()
	void HandleClicked();
};

UCLASS()
class MOBAPROJECT_API UMobaShopHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UMobaShopHUD(const FObjectInitializer& ObjectInitializer);

	void SetOwnerCharacter(AMobaBaseCharacter* InOwner);
	void PlaceInViewport();
	void SetShopOpen(bool bOpen);
	void RequestBuy(int32 OfferIndex);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void RebuildOffers();
	void UpdateOffers();

	UPROPERTY()
	TObjectPtr<AMobaBaseCharacter> OwnerCharacter;

	UPROPERTY()
	TObjectPtr<UTextBlock> GoldText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<class UVerticalBox> OfferList;

	UPROPERTY()
	TArray<TObjectPtr<UMobaShopBuyButton>> BuyButtons;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> CostTexts;
};
