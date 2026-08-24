#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "MobaInventoryHUD.generated.h"

class AMobaBaseCharacter;
class UTextBlock;
class UVerticalBox;

UCLASS()
class MOBAPROJECT_API UMobaInventoryHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UMobaInventoryHUD(const FObjectInitializer& ObjectInitializer);

	void SetOwnerCharacter(AMobaBaseCharacter* InOwner);
	void PlaceInViewport();
	void SetInventoryOpen(bool bOpen);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void RebuildList();
	void UpdateList();

	UPROPERTY()
	TObjectPtr<AMobaBaseCharacter> OwnerCharacter;

	UPROPERTY()
	TObjectPtr<UVerticalBox> ItemList;

	UPROPERTY()
	TObjectPtr<UTextBlock> EmptyText;

	FString LastSignature;
};
