#include "GA_MobaSkillshot.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "MobaBaseCharacter.h"
#include "MobaProjectile.h"

UGA_MobaSkillshot::UGA_MobaSkillshot()
{
	CooldownTag = FGameplayTag::RequestGameplayTag(FName("Cooldown.Skillshot"), false);
	Cooldown = 1.5f;
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
	Character->BeginPlantedAbility();

	UAbilityTask_WaitGameplayEvent* WaitFire = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		FGameplayTag::RequestGameplayTag(FName("Event.Skillshot.Fire"), false),
		nullptr,
		true);
	WaitFire->EventReceived.AddDynamic(this, &UGA_MobaSkillshot::OnSkillshotFire);
	WaitFire->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* PlayMontage = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		SkillshotMontage);
	PlayMontage->OnCompleted.AddDynamic(this, &UGA_MobaSkillshot::OnMontageDone);
	PlayMontage->OnBlendOut.AddDynamic(this, &UGA_MobaSkillshot::OnMontageDone);
	PlayMontage->OnInterrupted.AddDynamic(this, &UGA_MobaSkillshot::OnMontageDone);
	PlayMontage->OnCancelled.AddDynamic(this, &UGA_MobaSkillshot::OnMontageDone);
	PlayMontage->ReadyForActivation();
}

void UGA_MobaSkillshot::OnSkillshotFire(FGameplayEventData Payload)
{
	if (bFiredThisCast)
	{
		return;
	}
	bFiredThisCast = true;

	if (CurrentActorInfo && CurrentActorInfo->IsLocallyControlled())
	{
		SpawnBolt(true);
	}
	if (HasAuthority(&CurrentActivationInfo))
	{
		SpawnBolt(false);
	}
}

void UGA_MobaSkillshot::OnMontageDone()
{
	if (bEndedThisCast)
	{
		return;
	}
	bEndedThisCast = true;
	if (AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo()))
	{
		Character->EndPlantedAbility();
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_MobaSkillshot::SpawnBolt(bool bCosmetic)
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !ProjectileClass)
	{
		return;
	}

	const FVector Dir = Character->GetControlRotation().Vector();
	const FVector Start = Character->GetActorLocation() + FVector(0.f, 0.f, 40.f) + Dir * 80.f;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = Character;
	Params.Instigator = Character;

	AMobaProjectile* Bolt = Character->GetWorld()->SpawnActor<AMobaProjectile>(
		ProjectileClass,
		Start,
		Dir.Rotation(),
		Params);
	if (Bolt)
	{
		Bolt->InitFlight(Dir, Speed, Damage, Lifetime, bCosmetic);
	}
}
