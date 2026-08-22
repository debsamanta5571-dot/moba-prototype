#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "CoreMinimal.h"
#include "MobaAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class MOBAPROJECT_API UMobaAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UMobaAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Moba")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UMobaAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Moba")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UMobaAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Energy, Category = "Moba")
	FGameplayAttributeData Energy;
	ATTRIBUTE_ACCESSORS(UMobaAttributeSet, Energy)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxEnergy, Category = "Moba")
	FGameplayAttributeData MaxEnergy;
	ATTRIBUTE_ACCESSORS(UMobaAttributeSet, MaxEnergy)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealthRegen, Category = "Moba")
	FGameplayAttributeData HealthRegen;
	ATTRIBUTE_ACCESSORS(UMobaAttributeSet, HealthRegen)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_EnergyRegen, Category = "Moba")
	FGameplayAttributeData EnergyRegen;
	ATTRIBUTE_ACCESSORS(UMobaAttributeSet, EnergyRegen)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Gold, Category = "Moba")
	FGameplayAttributeData Gold;
	ATTRIBUTE_ACCESSORS(UMobaAttributeSet, Gold)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_GoldOnKill, Category = "Moba")
	FGameplayAttributeData GoldOnKill;
	ATTRIBUTE_ACCESSORS(UMobaAttributeSet, GoldOnKill)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageModifier, Category = "Moba")
	FGameplayAttributeData DamageModifier;
	ATTRIBUTE_ACCESSORS(UMobaAttributeSet, DamageModifier)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CooldownReduction, Category = "Moba")
	FGameplayAttributeData CooldownReduction;
	ATTRIBUTE_ACCESSORS(UMobaAttributeSet, CooldownReduction)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageResistance, Category = "Moba")
	FGameplayAttributeData DamageResistance;
	ATTRIBUTE_ACCESSORS(UMobaAttributeSet, DamageResistance)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeed, Category = "Moba")
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UMobaAttributeSet, MoveSpeed)

	static const UMobaAttributeSet* GetFromActor(const AActor* Actor);

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	void OnRep_Energy(const FGameplayAttributeData& OldEnergy);

	UFUNCTION()
	void OnRep_MaxEnergy(const FGameplayAttributeData& OldMaxEnergy);

	UFUNCTION()
	void OnRep_HealthRegen(const FGameplayAttributeData& OldHealthRegen);

	UFUNCTION()
	void OnRep_EnergyRegen(const FGameplayAttributeData& OldEnergyRegen);

	UFUNCTION()
	void OnRep_Gold(const FGameplayAttributeData& OldGold);

	UFUNCTION()
	void OnRep_GoldOnKill(const FGameplayAttributeData& OldGoldOnKill);

	UFUNCTION()
	void OnRep_DamageModifier(const FGameplayAttributeData& OldDamageModifier);

	UFUNCTION()
	void OnRep_CooldownReduction(const FGameplayAttributeData& OldCooldownReduction);

	UFUNCTION()
	void OnRep_DamageResistance(const FGameplayAttributeData& OldDamageResistance);

	UFUNCTION()
	void OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed);
};
