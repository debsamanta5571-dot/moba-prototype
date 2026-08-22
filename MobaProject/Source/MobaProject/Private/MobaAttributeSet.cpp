#include "MobaAttributeSet.h"
#include "AbilitySystemInterface.h"
#include "MobaBaseCharacter.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UObjectHash.h"

UMobaAttributeSet::UMobaAttributeSet()
{
	InitMaxHealth(100.f);
	InitHealth(100.f);
	InitMaxEnergy(100.f);
	InitEnergy(100.f);
	InitHealthRegen(2.f);
	InitEnergyRegen(5.f);
	InitGold(0.f);
	InitGoldOnKill(0.f);
	InitDamageModifier(1.f);
	InitCooldownReduction(0.f);
	InitDamageResistance(0.f);
	InitMoveSpeed(500.f);
}

const UMobaAttributeSet* UMobaAttributeSet::GetFromActor(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor))
	{
		if (const UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
		{
			if (const UMobaAttributeSet* Set = ASC->GetSet<UMobaAttributeSet>())
			{
				return Set;
			}
		}
	}

	TArray<UObject*> Children;
	GetObjectsWithOuter(Actor, Children);
	for (const UObject* Obj : Children)
	{
		if (const UMobaAttributeSet* Set = Cast<UMobaAttributeSet>(Obj))
		{
			return Set;
		}
	}
	return nullptr;
}

void UMobaAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UMobaAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMobaAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMobaAttributeSet, Energy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMobaAttributeSet, MaxEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMobaAttributeSet, HealthRegen, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMobaAttributeSet, EnergyRegen, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMobaAttributeSet, Gold, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMobaAttributeSet, GoldOnKill, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMobaAttributeSet, DamageModifier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMobaAttributeSet, CooldownReduction, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMobaAttributeSet, DamageResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMobaAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
}

void UMobaAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMobaAttributeSet, Health, OldHealth);
}

void UMobaAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMobaAttributeSet, MaxHealth, OldMaxHealth);
}

void UMobaAttributeSet::OnRep_Energy(const FGameplayAttributeData& OldEnergy)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMobaAttributeSet, Energy, OldEnergy);
}

void UMobaAttributeSet::OnRep_MaxEnergy(const FGameplayAttributeData& OldMaxEnergy)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMobaAttributeSet, MaxEnergy, OldMaxEnergy);
}

void UMobaAttributeSet::OnRep_HealthRegen(const FGameplayAttributeData& OldHealthRegen)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMobaAttributeSet, HealthRegen, OldHealthRegen);
}

void UMobaAttributeSet::OnRep_EnergyRegen(const FGameplayAttributeData& OldEnergyRegen)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMobaAttributeSet, EnergyRegen, OldEnergyRegen);
}

void UMobaAttributeSet::OnRep_Gold(const FGameplayAttributeData& OldGold)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMobaAttributeSet, Gold, OldGold);
}

void UMobaAttributeSet::OnRep_GoldOnKill(const FGameplayAttributeData& OldGoldOnKill)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMobaAttributeSet, GoldOnKill, OldGoldOnKill);
}

void UMobaAttributeSet::OnRep_DamageModifier(const FGameplayAttributeData& OldDamageModifier)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMobaAttributeSet, DamageModifier, OldDamageModifier);
}

void UMobaAttributeSet::OnRep_CooldownReduction(const FGameplayAttributeData& OldCooldownReduction)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMobaAttributeSet, CooldownReduction, OldCooldownReduction);
}

void UMobaAttributeSet::OnRep_DamageResistance(const FGameplayAttributeData& OldDamageResistance)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMobaAttributeSet, DamageResistance, OldDamageResistance);
}

void UMobaAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMobaAttributeSet, MoveSpeed, OldMoveSpeed);
	if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(GetOwningActor()))
	{
		Hero->RefreshMoveSpeed();
	}
}
