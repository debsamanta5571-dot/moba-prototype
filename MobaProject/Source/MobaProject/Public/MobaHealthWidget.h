#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "MobaHealthWidget.generated.h"

class AMobaBaseCharacter;
class UProgressBar;

UCLASS()
class MOBAPROJECT_API UMobaHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwnerCharacter(AMobaBaseCharacter* InOwner);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY()
	TObjectPtr<AMobaBaseCharacter> OwnerCharacter;

	UPROPERTY()
	TObjectPtr<UProgressBar> Bar;
};
