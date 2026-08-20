#include "MobaAttributeSet.h"
#include "Net/UnrealNetwork.h"

UMobaAttributeSet::UMobaAttributeSet()
{
	InitMaxHealth(100.f);
	InitHealth(100.f);
}

void UMobaAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UMobaAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMobaAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UMobaAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMobaAttributeSet, Health, OldHealth);
}

void UMobaAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMobaAttributeSet, MaxHealth, OldMaxHealth);
}
