#include "GA_MobaSkillshot.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "AnimNotify_SkillshotFire.h"
#include "Engine/World.h"
#include "MobaBaseCharacter.h"
#include "MobaProjectile.h"
#include "TimerManager.h"

namespace
{
	float SkillshotFireDelay(UAnimMontage* Montage)
	{
		if (Montage)
		{
			for (const FAnimNotifyEvent& Event : Montage->Notifies)
			{
				if (Event.Notify && Event.Notify->IsA(UAnimNotify_SkillshotFire::StaticClass()))
				{
					return FMath::Max(0.f, Event.GetTriggerTime());
				}
			}
		}
		return 0.2f;
	}
}

UGA_MobaSkillshot::UGA_MobaSkillshot()
{
	CooldownTag = FGameplayTag::RequestGameplayTag(FName("Cooldown.Skillshot"), false);
	Cooldown = 1.5f;
	EnergyCost = 25.f;
	DefaultCastSfx = EMobaSfx::SkillshotFire;
}

void UGA_MobaSkillshot::ActivateAbility(
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
	if (!Character || !ProjectileClass || !SkillshotMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bFiredThisCast = false;
	bEndedThisCast = false;
	ApplyMobaCooldown();
	Character->BeginPlantedAbility(1.5f);

	UAbilityTask_PlayMontageAndWait* PlayMontage = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		SkillshotMontage);
	PlayMontage->OnCompleted.AddDynamic(this, &UGA_MobaSkillshot::OnMontageDone);
	PlayMontage->OnBlendOut.AddDynamic(this, &UGA_MobaSkillshot::OnMontageDone);
	PlayMontage->OnInterrupted.AddDynamic(this, &UGA_MobaSkillshot::OnMontageDone);
	PlayMontage->OnCancelled.AddDynamic(this, &UGA_MobaSkillshot::OnMontageDone);
	PlayMontage->ReadyForActivation();

	const float Delay = SkillshotFireDelay(SkillshotMontage);
	if (UWorld* World = Character->GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimer);
		if (Delay <= KINDA_SMALL_NUMBER)
		{
			FireShot();
		}
		else
		{
			World->GetTimerManager().SetTimer(FireTimer, this, &UGA_MobaSkillshot::FireShot, Delay, false);
		}
	}
}

void UGA_MobaSkillshot::FireShot()
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character || bFiredThisCast || !Character->TryClaimVfx(TEXT("Skillshot")))
	{
		return;
	}
	bFiredThisCast = true;
	PlayCastSfx();

	const bool bLocal = Character->IsLocallyControlled();
	const bool bAuth = Character->HasAuthority();
	if (bAuth)
	{
		SpawnBolt(false, false);
	}
	else if (bLocal)
	{
		SpawnBolt(true, false);
	}
}

void UGA_MobaSkillshot::OnMontageDone()
{
	if (!bFiredThisCast)
	{
		FireShot();
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_MobaSkillshot::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (!bEndedThisCast)
	{
		bEndedThisCast = true;
		if (AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo()))
		{
			if (UWorld* World = Character->GetWorld())
			{
				World->GetTimerManager().ClearTimer(FireTimer);
			}
			Character->EndPlantedAbility();
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_MobaSkillshot::SpawnBolt(bool bCosmetic, bool bHideVisuals)
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !ProjectileClass)
	{
		return;
	}

	const FVector Dir = Character->GetControlRotation().Vector();
	const FVector Start = Character->GetActorLocation() + FVector(0.f, 0.f, 40.f) + Dir * 80.f;

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Instigator = Character;

	AMobaProjectile* Bolt = World->SpawnActor<AMobaProjectile>(
		ProjectileClass,
		Start,
		Dir.Rotation(),
		Params);
	if (Bolt)
	{
		Bolt->InitFlight(
			Dir,
			Speed,
			Damage,
			Lifetime,
			bCosmetic,
			bCosmetic ? TArray<FMobaEffectSpec>() : Effects,
			bHideVisuals);
	}
}
