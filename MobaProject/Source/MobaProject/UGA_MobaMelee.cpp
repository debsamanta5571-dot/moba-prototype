#include "UGA_MobaMelee.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "MobaBaseCharacter.h"

UUGA_MobaMelee::UUGA_MobaMelee()
{
	CooldownTag = FGameplayTag::RequestGameplayTag(FName("Cooldown.Melee"), false);
	Cooldown = 1.f;
	EnergyCost = 10.f;
	DefaultCastSfx = EMobaSfx::MeleeCast;
	DefaultHitSfx = EMobaSfx::MeleeHit;
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
	PlayCastSfx();
	Character->BeginPlantedAbility(1.5f);

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

	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	const FRotator Yaw(0.f, Character->GetControlRotation().Yaw, 0.f);
	const FVector Start = Character->GetActorLocation();
	const FVector End = Start + Yaw.Vector() * Range;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Character);

	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);

	TArray<FHitResult> Hits;
	Character->GetWorld()->SweepMultiByObjectType(
		Hits,
		Start,
		End,
		FQuat::Identity,
		Objects,
		FCollisionShape::MakeSphere(Radius),
		Params);

	TSet<AActor*> Damaged;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!IsValid(Target) || Target == Character || Damaged.Contains(Target))
		{
			continue;
		}
		if (ApplyAbilityHit(Target, Damage, Damaged.Num() == 0))
		{
			Damaged.Add(Target);
		}
	}
}

void UUGA_MobaMelee::OnMontageDone()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UUGA_MobaMelee::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (!bEndedThisSwing)
	{
		bEndedThisSwing = true;
		if (AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo()))
		{
			Character->EndPlantedAbility();
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
