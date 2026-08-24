#include "GA_MobaSkillshot.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "MobaBaseCharacter.h"
#include "MobaProjectile.h"
#include "TimerManager.h"

UGA_MobaSkillshot::UGA_MobaSkillshot()
{
	Cooldown = 1.5f;
	EnergyCost = 25.f;
	DefaultCastSfx = EMobaSfx::SkillshotFire;
	AnimNotifyTag = FGameplayTag::RequestGameplayTag(FName("Ability.2"), false);
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

	const FGameplayTag NotifyTag = ResolveNotifyTag(Character);
	if (NotifyTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* WaitNotify = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			NotifyTag,
			nullptr,
			true);
		WaitNotify->EventReceived.AddDynamic(this, &UGA_MobaSkillshot::OnAnimNotify);
		WaitNotify->ReadyForActivation();
	}
}

void UGA_MobaSkillshot::OnAnimNotify(FGameplayEventData Payload)
{
	FireShot();
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
	const FVector Start = GetSpawnLocation(Character);

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
			bHideVisuals,
			bCanDamageTowers);
	}
}

FVector UGA_MobaSkillshot::GetSpawnLocation(AMobaBaseCharacter* Character) const
{
	if (!Character)
	{
		return FVector::ZeroVector;
	}

	FVector Base = Character->GetActorLocation();
	if (!SpawnSocket.IsNone())
	{
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			if (Mesh->DoesSocketExist(SpawnSocket))
			{
				Base = Mesh->GetSocketLocation(SpawnSocket);
			}
		}
	}

	const FRotator Yaw(0.f, Character->GetControlRotation().Yaw, 0.f);
	return Base + Yaw.RotateVector(SpawnOffset);
}
