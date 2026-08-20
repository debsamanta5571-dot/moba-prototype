#include "UGA_MobaMelee.h"
#include "CollisionShape.h"
#include "MobaBaseCharacter.h"

UUGA_MobaMelee::UUGA_MobaMelee()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Cooldown.Melee"), false));
}

void UUGA_MobaMelee::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (HasAuthority(&ActivationInfo))
	{
		const FRotator Yaw(0.f, Character->GetControlRotation().Yaw, 0.f);
		const FVector Start = Character->GetActorLocation();
		const FVector End = Start + Yaw.Vector() * Range;

		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(Character);

		if (Character->GetWorld()->SweepSingleByChannel(
			Hit,
			Start,
			End,
			FQuat::Identity,
			ECC_Pawn,
			FCollisionShape::MakeSphere(Radius),
			Params))
		{
			AMobaBaseCharacter::ApplyMobaDamage(Hit.GetActor(), Damage, Character);
		}
	}

	Character->StartMeleeCooldown(Cooldown);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
