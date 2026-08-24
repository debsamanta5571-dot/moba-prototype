#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "MobaSettingsWidget.generated.h"

class UButton;
class USlider;
class UTextBlock;

UCLASS()
class MOBAPROJECT_API UMobaSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void PlaceInViewport();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void OnVolumeChanged(float Value);

	UFUNCTION()
	void OnGraphicsLow();

	UFUNCTION()
	void OnGraphicsMedium();

	UFUNCTION()
	void OnGraphicsHigh();

	UFUNCTION()
	void OnGraphicsEpic();

	UFUNCTION()
	void OnBackClicked();

	void UpdateVolumeLabel(float Volume);
	void SetGraphicsQuality(int32 Level);
	void RefreshGraphicsButtons();

	UPROPERTY()
	TObjectPtr<USlider> VolumeSlider;

	UPROPERTY()
	TObjectPtr<UTextBlock> VolumeValueText;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> GraphicsButtons;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> GraphicsLabels;

	UPROPERTY()
	TObjectPtr<UButton> BackButton;
};
