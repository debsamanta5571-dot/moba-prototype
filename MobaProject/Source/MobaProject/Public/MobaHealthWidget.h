#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "MobaHealthWidget.generated.h"

class AActor;
class AMobaBaseCharacter;
class UBorder;
class UProgressBar;
class UTextBlock;

UCLASS()
class MOBAPROJECT_API UMobaHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwnerActor(AActor* InOwner);
	void SetOwnerCharacter(AMobaBaseCharacter* InOwner);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void UpdateStatus();

	UPROPERTY()
	TObjectPtr<AActor> OwnerActor;

	UPROPERTY()
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY()
	TObjectPtr<UBorder> StatusFrame;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;
};
