#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "MobaAbilityHUD.generated.h"

class AMobaBaseCharacter;
class UBorder;
class UHorizontalBox;
class UImage;
class UProgressBar;
class UTextBlock;

UCLASS()
class MOBAPROJECT_API UMobaAbilityHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UMobaAbilityHUD(const FObjectInitializer& ObjectInitializer);

	void SetOwnerCharacter(AMobaBaseCharacter* InOwner);
	void PlaceInViewport();
	void ShowNotice(const FString& Message);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void RebuildSlots();
	void FillAbilityIcons();
	void UpdateSlots();
	void UpdateHealth();
	void UpdateEnergy();
	void UpdateStatus();
	void UpdatePing();

	UPROPERTY()
	TObjectPtr<AMobaBaseCharacter> OwnerCharacter;

	UPROPERTY()
	TObjectPtr<UBorder> StatusFrame;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY()
	TObjectPtr<UProgressBar> EnergyBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> EnergyText;

	UPROPERTY()
	TObjectPtr<UBorder> NoticeFrame;

	UPROPERTY()
	TObjectPtr<UTextBlock> NoticeText;

	float NoticeUntilTime = 0.f;

	UPROPERTY()
	TObjectPtr<UBorder> PingFrame;

	UPROPERTY()
	TObjectPtr<UTextBlock> PingText;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> SlotRow;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> Icons;

	UPROPERTY()
	TArray<TObjectPtr<UProgressBar>> CooldownBars;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> TimeTexts;
};
