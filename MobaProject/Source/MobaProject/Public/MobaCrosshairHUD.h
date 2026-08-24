#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "MobaCrosshairHUD.generated.h"

UCLASS()
class MOBAPROJECT_API UMobaCrosshairHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	void PlaceInViewport();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
};
