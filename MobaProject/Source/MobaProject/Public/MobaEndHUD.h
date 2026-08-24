#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "MobaEndHUD.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class MOBAPROJECT_API UMobaEndHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UMobaEndHUD(const FObjectInitializer& ObjectInitializer);

	void ShowResult(bool bVictory);
	void ShowLoading(const FString& Message);
	void PlaceInViewport();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	void RequestTravel(bool bPlayAgain);

	UFUNCTION()
	void OnPlayAgainClicked();

	UFUNCTION()
	void OnMenuClicked();

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UButton> PlayAgainButton;

	UPROPERTY()
	TObjectPtr<UButton> MenuButton;

	bool bVictory = false;
};
