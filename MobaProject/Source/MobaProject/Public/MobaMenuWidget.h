#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "MobaMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

UCLASS()
class MOBAPROJECT_API UMobaMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetNotice(const FString& Message);
	void FocusJoinAddress();
	FString GetJoinAddress() const;
	FString GetJoinHost() const;
	int32 GetJoinPort() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnHostClicked();

	UFUNCTION()
	void OnJoinClicked();

	UFUNCTION()
	void OnJoinAddressCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnSettingsClicked();

	UPROPERTY()
	TObjectPtr<UButton> HostButton;

	UPROPERTY()
	TObjectPtr<UButton> JoinButton;

	UPROPERTY()
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> AddressBox;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> PortBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> NoticeText;
};
