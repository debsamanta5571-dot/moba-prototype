#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "MobaLoadingWidget.generated.h"

class UTextBlock;

UCLASS()
class MOBAPROJECT_API UMobaLoadingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void PlaceInViewport();
	void SetMessage(const FString& Message);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	FString PendingMessage = TEXT("LOADING...");
	float Age = 0.f;
};
