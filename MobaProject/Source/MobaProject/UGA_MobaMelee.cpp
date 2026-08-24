#include "UGA_MobaMelee.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "MobaBaseCharacter.h"

UUGA_MobaMelee::UUGA_MobaMelee()
{
	Cooldown = 1.f;
	EnergyCost = 10.f;
	DefaultCastSfx = EMobaSfx::MeleeCast;
	DefaultHitSfx = EMobaSfx::MeleeHit;
	AnimNotifyTag = FGameplayTag::RequestGameplayTag(FName("Ability.1"), false);
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
	ShowRangeRing();

	UAbilityTask_WaitGameplayEvent* WaitHit = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		ResolveNotifyTag(Character),
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

void UUGA_MobaMelee::ShowRangeRing()
{
	const bool bFireMontage = MeleeMontage && MeleeMontage->GetName().Contains(TEXT("Fire"));
	if (!bShowRangeRing && !bFireMontage)
	{
		return;
	}

	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	const float RingRadius = FMath::Max(Range + Radius, 40.f);
	const float Lifetime = FMath::Max(RangeRingLifetime, 0.15f);
	Character->PlayFireRingDebug(RingRadius, Lifetime);
	if (HasAuthority(&CurrentActivationInfo))
	{
		Character->MulticastFireRingVfx(RingRadius, Lifetime);
	}
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

	PlayHitSfx(Character->GetActorLocation() + Character->GetActorForwardVector() * 80.f);

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

	Hits.Sort([](const FHitResult& A, const FHitResult& B)
	{
		return A.Distance < B.Distance;
	});

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
			if (Damaged.Num() >= FMath::Max(1, MaxTargets))
			{
				break;
			}
		}
	}
}

void UUGA_MobaMelee::OnMontageDone()
{
	if (!bHitThisSwing)
	{
		TryHit();
	}
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
