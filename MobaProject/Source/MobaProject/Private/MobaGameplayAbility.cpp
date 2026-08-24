#include "MobaGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "MobaBaseCharacter.h"
#include "MobaTower.h"

UMobaGameplayAbility::UMobaGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bRetriggerInstancedAbility = false;
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead"), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned"), false));
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
		const FGameplayTag ActiveCooldown = ResolveCooldownTag(ActorInfo->AvatarActor.Get());
		if (ActiveCooldown.IsValid() && ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(ActiveCooldown))
		{
			return false;
		}
		const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName("State.Dead"), false);
		if (DeadTag.IsValid() && ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(DeadTag))
		{
			return false;
		}
		const FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(FName("State.Stunned"), false);
		if (StunTag.IsValid() && ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(StunTag))
		{
			return false;
		}
	}

	if (const AMobaBaseCharacter* Avatar = Cast<AMobaBaseCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (Avatar->IsStunned() || Avatar->IsDead() || !Avatar->HasEnergy(EnergyCost))
		{
			return false;
		}
	}

	return true;
}

bool UMobaGameplayAbility::ApplyAbilityHit(AActor* Target, float InDamage, bool bApplySelfEffects) const
{
	if (!bCanDamageTowers && Cast<AMobaTower>(Target))
	{
		return false;
	}

	AActor* Source = GetAvatarActorFromActorInfo();
	const bool bHit = AMobaBaseCharacter::ApplyMobaDamage(Target, InDamage, Source);
	if (!bHit)
	{
		return false;
	}

	AMobaBaseCharacter::ApplyMobaEffects(Target, Source, Effects, EMobaEffectTarget::HitActor);
	if (bApplySelfEffects)
	{
		AMobaBaseCharacter::ApplyMobaEffects(Target, Source, Effects, EMobaEffectTarget::Self);
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

void UMobaGameplayAbility::PlayCastSfx() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	PlayCastSfxAt(Avatar ? Avatar->GetActorLocation() : FVector::ZeroVector);
}

void UMobaGameplayAbility::PlayCastSfxAt(const FVector& Location) const
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (Character)
	{
		Character->PlayAbilitySfx(CastSound, DefaultCastSfx, Location);
	}
}

void UMobaGameplayAbility::PlayHitSfx(const FVector& Location) const
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (Character)
	{
		Character->PlayAbilitySfx(HitSound, DefaultHitSfx, Location);
	}
}

FGameplayTag UMobaGameplayAbility::ResolveCooldownTag(const AActor* Avatar) const
{
	if (const AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(Avatar))
	{
		const FGameplayTag SlotTag = Character->GetCooldownTagForAbilityClass(GetClass());
		if (SlotTag.IsValid())
		{
			return SlotTag;
		}
	}
	return FGameplayTag();
}

FGameplayTag UMobaGameplayAbility::ResolveNotifyTag(const AActor* Avatar) const
{
	if (AnimNotifyTag.IsValid())
	{
		return AnimNotifyTag;
	}
	return ResolveCooldownTag(Avatar);
}

void UMobaGameplayAbility::ApplyMobaCooldown() const
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	Character->StartCooldown(ResolveCooldownTag(Character), Cooldown);
	if (HasAuthority(&CurrentActivationInfo))
	{
		Character->SpendEnergy(EnergyCost);
	}
}
