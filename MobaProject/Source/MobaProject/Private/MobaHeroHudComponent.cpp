#include "MobaHeroHudComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "MobaAbilityHUD.h"
#include "MobaBaseCharacter.h"
#include "MobaDescComponent.h"
#include "MobaGameInstance.h"
#include "MobaGoldHUD.h"
#include "MobaInventoryHUD.h"
#include "MobaRespawnHUD.h"
#include "MobaShopHUD.h"
#include "Components/WidgetComponent.h"

UMobaHeroHudComponent::UMobaHeroHudComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

AMobaBaseCharacter* UMobaHeroHudComponent::GetHero() const
{
	return Cast<AMobaBaseCharacter>(GetOwner());
}

void UMobaHeroHudComponent::CreateHud()
{
	CreateAbilityHUD();
}

void UMobaHeroHudComponent::TickLocal()
{
	if (RespawnHUD)
	{
		RespawnHUD->Refresh();
	}
}

void UMobaHeroHudComponent::ShowEnergyNotice()
{
	AMobaBaseCharacter* Hero = GetHero();
	if (!Hero || !Hero->IsLocallyControlled() || !AbilityHUD)
	{
		return;
	}
	AbilityHUD->ShowNotice(TEXT("Not enough energy"));
}

void UMobaHeroHudComponent::NotifyDeath()
{
	CreateRespawnHUD();
	TickLocal();
}

void UMobaHeroHudComponent::NotifyAlive()
{
	TickLocal();
}

void UMobaHeroHudComponent::RefreshCrosshairVisibility()
{
	AMobaBaseCharacter* Hero = GetHero();
	if (!Hero || !Hero->GetCrosshair())
	{
		return;
	}
	const bool bShow = Hero->IsLocallyControlled() && !Hero->IsDead();
	Hero->GetCrosshair()->SetHiddenInGame(!bShow);
	Hero->GetCrosshair()->SetVisibility(bShow);
}

void UMobaHeroHudComponent::CreateAbilityHUD()
{
	AMobaBaseCharacter* Hero = GetHero();
	if (!Hero || !Hero->IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Hero->GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	RefreshCrosshairVisibility();
	CreateGoldHUD();
	CreateShopHUD();
	CreateInventoryHUD();
	CreateRespawnHUD();

	if (AbilityHUD)
	{
		AbilityHUD->PlaceInViewport();
		if (Hero->GetHealthWidget())
		{
			Hero->GetHealthWidget()->SetHiddenInGame(true);
		}
		return;
	}

	AbilityHUD = CreateWidget<UMobaAbilityHUD>(PC, UMobaAbilityHUD::StaticClass());
	if (!AbilityHUD)
	{
		return;
	}
	AbilityHUD->SetOwnerCharacter(Hero);
	AbilityHUD->PlaceInViewport();
	if (Hero->GetHealthWidget())
	{
		Hero->GetHealthWidget()->SetHiddenInGame(true);
	}
}

void UMobaHeroHudComponent::CreateRespawnHUD()
{
	AMobaBaseCharacter* Hero = GetHero();
	if (!Hero || !Hero->IsLocallyControlled())
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Hero->GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (RespawnHUD)
	{
		RespawnHUD->SetOwnerCharacter(Hero);
		RespawnHUD->PlaceInViewport();
		RespawnHUD->Refresh();
		return;
	}
	RespawnHUD = CreateWidget<UMobaRespawnHUD>(PC, UMobaRespawnHUD::StaticClass());
	if (!RespawnHUD)
	{
		return;
	}
	RespawnHUD->SetOwnerCharacter(Hero);
	RespawnHUD->PlaceInViewport();
}

void UMobaHeroHudComponent::CreateGoldHUD()
{
	AMobaBaseCharacter* Hero = GetHero();
	if (!Hero || !Hero->IsLocallyControlled())
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Hero->GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (GoldHUD)
	{
		GoldHUD->PlaceInViewport();
		return;
	}
	GoldHUD = CreateWidget<UMobaGoldHUD>(PC, UMobaGoldHUD::StaticClass());
	if (!GoldHUD)
	{
		return;
	}
	GoldHUD->SetOwnerCharacter(Hero);
	GoldHUD->PlaceInViewport();
}

void UMobaHeroHudComponent::CreateShopHUD()
{
	AMobaBaseCharacter* Hero = GetHero();
	if (!Hero || !Hero->IsLocallyControlled())
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Hero->GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (ShopHUD)
	{
		ShopHUD->PlaceInViewport();
		return;
	}
	ShopHUD = CreateWidget<UMobaShopHUD>(PC, UMobaShopHUD::StaticClass());
	if (!ShopHUD)
	{
		return;
	}
	ShopHUD->SetOwnerCharacter(Hero);
	ShopHUD->PlaceInViewport();
	ShopHUD->SetShopOpen(false);
}

void UMobaHeroHudComponent::CreateInventoryHUD()
{
	AMobaBaseCharacter* Hero = GetHero();
	if (!Hero || !Hero->IsLocallyControlled())
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Hero->GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (InventoryHUD)
	{
		InventoryHUD->PlaceInViewport();
		return;
	}
	InventoryHUD = CreateWidget<UMobaInventoryHUD>(PC, UMobaInventoryHUD::StaticClass());
	if (!InventoryHUD)
	{
		return;
	}
	InventoryHUD->SetOwnerCharacter(Hero);
	InventoryHUD->PlaceInViewport();
	InventoryHUD->SetInventoryOpen(false);
}

void UMobaHeroHudComponent::ToggleShop()
{
	AMobaBaseCharacter* Hero = GetHero();
	if (!Hero || !Hero->IsLocallyControlled())
	{
		return;
	}
	if (Hero->GetAbilityDesc() && Hero->GetAbilityDesc()->IsOpen())
	{
		Hero->GetAbilityDesc()->SetOpen(false);
	}
	if (bShopOpen)
	{
		SetShopOpen(false);
		return;
	}
	if (!Hero->CanUseShop())
	{
		return;
	}
	CreateShopHUD();
	SetShopOpen(true);
}

void UMobaHeroHudComponent::ToggleInventory()
{
	AMobaBaseCharacter* Hero = GetHero();
	if (!Hero || !Hero->IsLocallyControlled())
	{
		return;
	}
	if (Hero->GetAbilityDesc() && Hero->GetAbilityDesc()->IsOpen())
	{
		Hero->GetAbilityDesc()->SetOpen(false);
	}
	CreateInventoryHUD();
	SetInventoryOpen(!bInventoryOpen);
}

void UMobaHeroHudComponent::ToggleDesc()
{
	AMobaBaseCharacter* Hero = GetHero();
	if (!Hero || !Hero->IsLocallyControlled() || !Hero->GetAbilityDesc())
	{
		return;
	}
	if (bShopOpen)
	{
		SetShopOpen(false);
	}
	if (bInventoryOpen)
	{
		SetInventoryOpen(false);
	}
	Hero->GetAbilityDesc()->ToggleOverlay();
}

void UMobaHeroHudComponent::ToggleSettings()
{
	AMobaBaseCharacter* Hero = GetHero();
	if (!Hero || !Hero->IsLocallyControlled())
	{
		return;
	}
	if (UMobaGameInstance* GI = Hero->GetGameInstance<UMobaGameInstance>())
	{
		GI->ToggleSettings();
	}
}

void UMobaHeroHudComponent::SetShopOpen(bool bOpen)
{
	AMobaBaseCharacter* Hero = GetHero();
	bShopOpen = bOpen && Hero && Hero->CanUseShop();
	if (ShopHUD)
	{
		ShopHUD->SetShopOpen(bShopOpen);
	}
	RefreshMenuInput();
}

void UMobaHeroHudComponent::SetInventoryOpen(bool bOpen)
{
	bInventoryOpen = bOpen;
	if (InventoryHUD)
	{
		InventoryHUD->SetInventoryOpen(bOpen);
	}
	RefreshMenuInput();
}

void UMobaHeroHudComponent::RefreshMenuInput()
{
	AMobaBaseCharacter* Hero = GetHero();
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (const UMobaGameInstance* GI = Hero->GetGameInstance<UMobaGameInstance>())
	{
		if (GI->IsShowingLoading() || GI->IsJoinLoadout() || GI->IsMenuUiActive())
		{
			return;
		}
	}

	const bool bNeedMouse = bShopOpen;
	PC->bShowMouseCursor = bNeedMouse;
	if (bNeedMouse)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		if (ShopHUD)
		{
			Mode.SetWidgetToFocus(ShopHUD->TakeWidget());
		}
		PC->SetInputMode(Mode);
		PC->SetIgnoreLookInput(true);
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetIgnoreLookInput(false);
	}
}

void UMobaHeroHudComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetShopOpen(false);
	SetInventoryOpen(false);
	if (AMobaBaseCharacter* Hero = GetHero())
	{
		if (Hero->GetAbilityDesc())
		{
			Hero->GetAbilityDesc()->SetOpen(false);
		}
	}
	if (AbilityHUD)
	{
		AbilityHUD->RemoveFromParent();
		AbilityHUD = nullptr;
	}
	if (GoldHUD)
	{
		GoldHUD->RemoveFromParent();
		GoldHUD = nullptr;
	}
	if (ShopHUD)
	{
		ShopHUD->RemoveFromParent();
		ShopHUD = nullptr;
	}
	if (InventoryHUD)
	{
		InventoryHUD->RemoveFromParent();
		InventoryHUD = nullptr;
	}
	if (RespawnHUD)
	{
		RespawnHUD->RemoveFromParent();
		RespawnHUD = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
