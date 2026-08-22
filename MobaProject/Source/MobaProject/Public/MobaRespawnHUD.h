#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "MobaRespawnHUD.generated.h"

class AMobaBaseCharacter;
class UTextBlock;

UCLASS()
class MOBAPROJECT_API UMobaRespawnHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UMobaRespawnHUD(const FObjectInitializer& ObjectInitializer);

	void SetOwnerCharacter(AMobaBaseCharacter* InOwner);
	void PlaceInViewport();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void Rebuild();
	void UpdateTimer();

	UPROPERTY()
	TObjectPtr<AMobaBaseCharacter> OwnerCharacter;

	UPROPERTY()
	TObjectPtr<UTextBlock> TimerText;
};
