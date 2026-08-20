#include "MobaGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "MobaBaseCharacter.h"

UMobaGameplayAbility::UMobaGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead"), false));
}

bool UMobaGameplayAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		if (CooldownTag.IsValid() && ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(CooldownTag))
		{
			return false;
		}
		const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName("State.Dead"), false);
		if (DeadTag.IsValid() && ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(DeadTag))
		{
			return false;
		}
	}

	return true;
}

FGameplayAbilityTargetDataHandle UMobaGameplayAbility::MakeDirectionTargetData(const FVector& Direction)
{
	FGameplayAbilityTargetData_LocationInfo* Data = new FGameplayAbilityTargetData_LocationInfo();
	Data->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	Data->TargetLocation.LiteralTransform = FTransform(Direction);

	FGameplayAbilityTargetDataHandle Handle;
	Handle.Add(Data);
	return Handle;
}

FGameplayAbilityTargetDataHandle UMobaGameplayAbility::MakeLocationTargetData(const FVector& Location)
{
	FGameplayAbilityTargetData_LocationInfo* Data = new FGameplayAbilityTargetData_LocationInfo();
	Data->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	Data->TargetLocation.LiteralTransform = FTransform(Location);

	FGameplayAbilityTargetDataHandle Handle;
	Handle.Add(Data);
	return Handle;
}

void UMobaGameplayAbility::BeginHold(AMobaBaseCharacter* Avatar) const
{
}

void UMobaGameplayAbility::ConfirmHold(AMobaBaseCharacter* Avatar) const
{
}

void UMobaGameplayAbility::CancelHold(AMobaBaseCharacter* Avatar) const
{
}

void UMobaGameplayAbility::ApplyMobaCooldown() const
{
	if (AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo()))
	{
		Character->StartCooldown(CooldownTag, Cooldown);
	}
}
