#include "MobaShopComponent.h"
#include "EngineUtils.h"
#include "MobaAttributeSet.h"
#include "MobaBaseCharacter.h"
#include "MobaHeroHudComponent.h"
#include "MobaShop.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UMobaShopComponent::UMobaShopComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	auto AddOffer = [this](const TCHAR* Name, EMobaShopStat Stat, float Magnitude, float Cost)
	{
		FMobaShopOffer Offer;
		Offer.Name = Name;
		Offer.Stat = Stat;
		Offer.Magnitude = Magnitude;
		Offer.Cost = Cost;
		Offers.Add(Offer);
	};
	AddOffer(TEXT("Damage"), EMobaShopStat::Damage, 0.1f, 100.f);
	AddOffer(TEXT("Energy"), EMobaShopStat::Energy, 20.f, 80.f);
	AddOffer(TEXT("CDR"), EMobaShopStat::CooldownReduction, 0.05f, 120.f);
	AddOffer(TEXT("Health"), EMobaShopStat::Health, 25.f, 80.f);
	AddOffer(TEXT("Resist"), EMobaShopStat::DamageResistance, 0.05f, 120.f);
	AddOffer(TEXT("Move Speed"), EMobaShopStat::MoveSpeed, 25.f, 80.f);
	AddOffer(TEXT("Gold Regen"), EMobaShopStat::GoldRegen, 1.f, 100.f);
}

void UMobaShopComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMobaShopComponent, bInShopRange);
	DOREPLIFETIME(UMobaShopComponent, PurchasedOffers);
}

AMobaBaseCharacter* UMobaShopComponent::GetHero() const
{
	return Cast<AMobaBaseCharacter>(GetOwner());
}

void UMobaShopComponent::AdoptLegacyOffers(const TArray<FMobaShopOffer>& InOffers)
{
	if (InOffers.Num() > 0)
	{
		Offers = InOffers;
	}
}

bool UMobaShopComponent::CanUse() const
{
	const AMobaBaseCharacter* Hero = GetHero();
	return Hero && (Hero->IsDead() || bInShopRange);
}

bool UMobaShopComponent::CanApplyOffer(const FMobaShopOffer& Offer) const
{
	const UMobaAttributeSet* Set = UMobaAttributeSet::GetFromActor(GetHero());
	if (!Set || Offer.Magnitude <= 0.f)
	{
		return false;
	}
	switch (Offer.Stat)
	{
	case EMobaShopStat::CooldownReduction:
		return Set->GetCooldownReduction() < 0.8f - KINDA_SMALL_NUMBER;
	case EMobaShopStat::DamageResistance:
		return Set->GetDamageResistance() < 0.9f - KINDA_SMALL_NUMBER;
	default:
		return true;
	}
}

float UMobaShopComponent::GetOfferCost(int32 Index) const
{
	if (!Offers.IsValidIndex(Index))
	{
		return 0.f;
	}
	const FString& Name = Offers[Index].Name;
	int32 Bought = 0;
	for (const FMobaShopOffer& BoughtOffer : PurchasedOffers)
	{
		if (BoughtOffer.Name == Name)
		{
			++Bought;
		}
	}
	return FMath::RoundToFloat(Offers[Index].Cost * FMath::Pow(1.2f, static_cast<float>(Bought)));
}

bool UMobaShopComponent::CanBuy(int32 Index) const
{
	const AMobaBaseCharacter* Hero = GetHero();
	if (!CanUse() || !Hero || !Offers.IsValidIndex(Index))
	{
		return false;
	}
	return Hero->GetGold() + 0.01f >= GetOfferCost(Index) && CanApplyOffer(Offers[Index]);
}

bool UMobaShopComponent::CanBuyAnything() const
{
	if (!CanUse())
	{
		return false;
	}
	for (int32 i = 0; i < Offers.Num(); ++i)
	{
		if (CanBuy(i))
		{
			return true;
		}
	}
	return false;
}

void UMobaShopComponent::TryBuy(int32 Index)
{
	if (!CanBuy(Index))
	{
		return;
	}
	ServerBuyOffer(Index);
}

void UMobaShopComponent::ServerBuyOffer_Implementation(int32 Index)
{
	AMobaBaseCharacter* Hero = GetHero();
	if (!CanBuy(Index) || !Hero)
	{
		return;
	}
	const FMobaShopOffer Offer = Offers[Index];
	Hero->SpendGold(GetOfferCost(Index));
	ApplyOffer(Offer);
	PurchasedOffers.Add(Offer);
}

void UMobaShopComponent::ApplyOffer(const FMobaShopOffer& Offer)
{
	AMobaBaseCharacter* Hero = GetHero();
	UMobaAttributeSet* Set = const_cast<UMobaAttributeSet*>(UMobaAttributeSet::GetFromActor(Hero));
	if (!Set)
	{
		return;
	}

	switch (Offer.Stat)
	{
	case EMobaShopStat::Damage:
		Set->SetDamageModifier(Set->GetDamageModifier() + Offer.Magnitude);
		break;
	case EMobaShopStat::Energy:
		Set->SetMaxEnergy(Set->GetMaxEnergy() + Offer.Magnitude);
		Set->SetEnergy(Set->GetEnergy() + Offer.Magnitude);
		break;
	case EMobaShopStat::CooldownReduction:
		Set->SetCooldownReduction(FMath::Clamp(Set->GetCooldownReduction() + Offer.Magnitude, 0.f, 0.8f));
		break;
	case EMobaShopStat::Health:
		Set->SetMaxHealth(Set->GetMaxHealth() + Offer.Magnitude);
		Set->SetHealth(Set->GetHealth() + Offer.Magnitude);
		break;
	case EMobaShopStat::DamageResistance:
		Set->SetDamageResistance(FMath::Clamp(Set->GetDamageResistance() + Offer.Magnitude, 0.f, 0.9f));
		break;
	case EMobaShopStat::MoveSpeed:
		Set->SetMoveSpeed(Set->GetMoveSpeed() + Offer.Magnitude);
		if (Hero)
		{
			Hero->RefreshMoveSpeed();
		}
		break;
	case EMobaShopStat::GoldRegen:
		Set->SetGoldRegen(Set->GetGoldRegen() + Offer.Magnitude);
		break;
	default:
		break;
	}
}

void UMobaShopComponent::RefreshRange()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bIgnoreRangeChanges)
	{
		return;
	}

	AMobaBaseCharacter* Hero = GetHero();
	if (!Hero)
	{
		return;
	}
	Hero->SyncTeamFromPlayerState();

	int32 Count = 0;
	const int32 HeroTeam = Hero->GetTeamId();
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AMobaShop> It(World); It; ++It)
		{
			AMobaShop* ShopActor = *It;
			if (!ShopActor)
			{
				continue;
			}
			const int32 ShopTeam = ShopActor->GetTeamId();
			if (ShopTeam != 0 && HeroTeam != 0 && ShopTeam != HeroTeam)
			{
				continue;
			}
			if (ShopActor->ContainsPawn(Hero))
			{
				++Count;
			}
		}
	}

	RangeCount = Count;
	const bool bNowIn = Count > 0;
	if (bInShopRange == bNowIn)
	{
		return;
	}
	bInShopRange = bNowIn;
	OnRep_InShopRange();
}

void UMobaShopComponent::ScheduleRangeRefresh()
{
	RefreshRange();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UMobaShopComponent::RefreshRange));
	}
}

void UMobaShopComponent::SetIgnoreRangeChanges(bool bIgnore)
{
	bIgnoreRangeChanges = bIgnore;
}

void UMobaShopComponent::OnRep_InShopRange()
{
	AMobaBaseCharacter* Hero = GetHero();
	if (!Hero || CanUse())
	{
		return;
	}
	if (UMobaHeroHudComponent* Hud = Hero->GetHeroHud())
	{
		Hud->SetShopOpen(false);
	}
}
