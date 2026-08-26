#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "MobaLobbyWidget.generated.h"

class UButton;
class USlider;
class UTextBlock;
class UVerticalBox;

UCLASS()
class MOBAPROJECT_API UMobaLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void PlaceInViewport();
	void SetJoinInProgress(bool bJoin);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void OnTeam1Clicked();

	UFUNCTION()
	void OnTeam2Clicked();

	UFUNCTION()
	void OnBrawlerClicked();

	UFUNCTION()
	void OnMageClicked();

	UFUNCTION()
	void OnStartClicked();

	UFUNCTION()
	void OnLeaveClicked();

	UFUNCTION()
	void OnPingMinChanged(float Value);

	UFUNCTION()
	void OnPingMaxChanged(float Value);

	void ApplyPingSliders();
	void UpdatePingLabel();

	void RequestTeam(int32 Team);
	void Refresh();
	void RefreshPlayerList();
	void RefreshTeamButtons();
	void RefreshHeroButtons();
	void RefreshPingPanel();

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StartLabel;

	UPROPERTY()
	TObjectPtr<UVerticalBox> PlayerList;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<UButton> Team1Button;

	UPROPERTY()
	TObjectPtr<UButton> Team2Button;

	UPROPERTY()
	TObjectPtr<UButton> BrawlerButton;

	UPROPERTY()
	TObjectPtr<UButton> MageButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> BrawlerLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> MageLabel;

	UPROPERTY()
	TObjectPtr<UButton> StartButton;

	UPROPERTY()
	TObjectPtr<UButton> LeaveButton;

	UPROPERTY()
	TObjectPtr<UVerticalBox> PingPanel;

	UPROPERTY()
	TObjectPtr<USlider> PingMinSlider;

	UPROPERTY()
	TObjectPtr<USlider> PingMaxSlider;

	UPROPERTY()
	TObjectPtr<UTextBlock> PingValueText;

	FString LastListSignature;
	FTimerHandle RefreshTimer;
	bool bJoinInProgress = false;
};
