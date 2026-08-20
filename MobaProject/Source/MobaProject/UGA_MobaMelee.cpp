#include "UGA_MobaMelee.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "CollisionShape.h"
#include "MobaBaseCharacter.h"

UUGA_MobaMelee::UUGA_MobaMelee()
{
	CooldownTag = FGameplayTag::RequestGameplayTag(FName("Cooldown.Melee"), false);
	Cooldown = 1.f;
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
	if (!Character || !MeleeMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bHitThisSwing = false;
	bEndedThisSwing = false;
	ApplyMobaCooldown();
	Character->BeginPlantedAbility();

	UAbilityTask_WaitGameplayEvent* WaitHit = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		FGameplayTag::RequestGameplayTag(FName("Event.Melee.Hit"), false),
		nullptr,
		true);
	WaitHit->EventReceived.AddDynamic(this, &UUGA_MobaMelee::OnMeleeHit);
	WaitHit->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* PlayMontage = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		MeleeMontage);
	PlayMontage->OnCompleted.AddDynamic(this, &UUGA_MobaMelee::OnMontageDone);
	PlayMontage->OnBlendOut.AddDynamic(this, &UUGA_MobaMelee::OnMontageDone);
	PlayMontage->OnInterrupted.AddDynamic(this, &UUGA_MobaMelee::OnMontageDone);
	PlayMontage->OnCancelled.AddDynamic(this, &UUGA_MobaMelee::OnMontageDone);
	PlayMontage->ReadyForActivation();
}

void UUGA_MobaMelee::OnMeleeHit(FGameplayEventData Payload)
{
	TryHit();
}

void UUGA_MobaMelee::TryHit()
{
	if (bHitThisSwing)
	{
		return;
	}
	bHitThisSwing = true;

	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

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

void UUGA_MobaMelee::OnMontageDone()
{
	if (bEndedThisSwing)
	{
		return;
	}
	bEndedThisSwing = true;
	if (AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo()))
	{
		Character->EndPlantedAbility();
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
