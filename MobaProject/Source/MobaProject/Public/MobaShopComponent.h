#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "MobaShopTypes.h"
#include "MobaShopComponent.generated.h"

class AMobaBaseCharacter;

// Fountain range, catalog, and gold spend. HUD only opens the panel.
UCLASS(ClassGroup = (Moba), meta = (BlueprintSpawnableComponent))
class MOBAPROJECT_API UMobaShopComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMobaShopComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void AdoptLegacyOffers(const TArray<FMobaShopOffer>& InOffers);

	const TArray<FMobaShopOffer>& GetOffers() const { return Offers; }
	const TArray<FMobaShopOffer>& GetPurchased() const { return PurchasedOffers; }
	bool IsInRange() const { return bInShopRange; }
	bool CanUse() const;
	bool CanBuyAnything() const;
	bool CanBuy(int32 Index) const;
	float GetOfferCost(int32 Index) const;
	void TryBuy(int32 Index);
	void RefreshRange();
	void ScheduleRangeRefresh();
	void SetIgnoreRangeChanges(bool bIgnore);
	float GetHealthRegenBonus() const { return HealthRegenBonus; }
	float GetEnergyRegenBonus() const { return EnergyRegenBonus; }

	UFUNCTION(Server, Reliable)
	void ServerBuyOffer(int32 Index);

protected:
	AMobaBaseCharacter* GetHero() const;
	bool CanApplyOffer(const FMobaShopOffer& Offer) const;
	void ApplyOffer(const FMobaShopOffer& Offer);

	UFUNCTION()
	void OnRep_InShopRange();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Shop")
	TArray<FMobaShopOffer> Offers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Shop", meta = (ClampMin = "0.0"))
	float HealthRegenBonus = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Shop", meta = (ClampMin = "0.0"))
	float EnergyRegenBonus = 45.f;

	UPROPERTY(ReplicatedUsing = OnRep_InShopRange)
	bool bInShopRange = false;

	UPROPERTY(Replicated)
	TArray<FMobaShopOffer> PurchasedOffers;

	int32 RangeCount = 0;
	bool bIgnoreRangeChanges = false;
};
