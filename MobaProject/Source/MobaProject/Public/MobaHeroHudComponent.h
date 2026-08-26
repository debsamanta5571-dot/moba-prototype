#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "MobaHeroHudComponent.generated.h"

class AMobaBaseCharacter;
class UMobaAbilityHUD;
class UMobaGoldHUD;
class UMobaInventoryHUD;
class UMobaRespawnHUD;
class UMobaShopHUD;

UCLASS(ClassGroup = (Moba), meta = (BlueprintSpawnableComponent))
class MOBAPROJECT_API UMobaHeroHudComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMobaHeroHudComponent();

	void CreateHud();
	void TickLocal();
	void ShowEnergyNotice();
	void ToggleShop();
	void ToggleInventory();
	void ToggleDesc();
	void ToggleSettings();
	void SetShopOpen(bool bOpen);
	void SetInventoryOpen(bool bOpen);
	void RefreshMenuInput();
	void RefreshCrosshairVisibility();
	void NotifyDeath();
	void NotifyAlive();
	bool IsShopOpen() const { return bShopOpen; }
	bool IsInventoryOpen() const { return bInventoryOpen; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	AMobaBaseCharacter* GetHero() const;
	void CreateAbilityHUD();
	void CreateGoldHUD();
	void CreateShopHUD();
	void CreateInventoryHUD();
	void CreateRespawnHUD();

	UPROPERTY()
	TObjectPtr<UMobaAbilityHUD> AbilityHUD;

	UPROPERTY()
	TObjectPtr<UMobaGoldHUD> GoldHUD;

	UPROPERTY()
	TObjectPtr<UMobaShopHUD> ShopHUD;

	UPROPERTY()
	TObjectPtr<UMobaInventoryHUD> InventoryHUD;

	UPROPERTY()
	TObjectPtr<UMobaRespawnHUD> RespawnHUD;

	bool bShopOpen = false;
	bool bInventoryOpen = false;
};
