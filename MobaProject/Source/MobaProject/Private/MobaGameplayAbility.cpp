#include "MobaGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "MobaBaseCharacter.h"
#include "MobaCombatLibrary.h"
#include "MobaTower.h"
#include "TimerManager.h"

UMobaGameplayAbility::UMobaGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bRetriggerInstancedAbility = false;
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead"), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned"), false));
	// GAS input triggers fire from InputID. We bind Enhanced Input ourselves, so leave this empty.
	AbilityTriggers.Reset();
}

void UMobaGameplayAbility::PostInitProperties()
{
	Super::PostInitProperties();
	AbilityTriggers.Reset();
}

void UMobaGameplayAbility::PostLoad()
{
	Super::PostLoad();
	AbilityTriggers.Reset();
}

void UMobaGameplayAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Do not Super: empty BP ActivateAbility graphs set bHasBlueprintActivate and end the ability.

	bCastNotifyFired = false;
	bCastMontageDone = false;
	bPlantedThisCast = false;

	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !PrepareCast(Character, TriggerEventData))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ApplyMobaCooldown();
	if (bPlayCastSfxOnStart)
	{
		PlayCastSfx();
	}
	if (bPlantOnCast)
	{
		Character->BeginPlantedAbility(GetPlantDuration());
		bPlantedThisCast = true;
	}

	OnCastStarted(Character);
	StartCastMontage();
}

// Montage wait + slot notify. No montage → fire the notify immediately so hits still go out.

void UMobaGameplayAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NotifyFallbackTimer);
		World->GetTimerManager().ClearTimer(MontageFallbackTimer);
	}
	EndCastPlant();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UMobaGameplayAbility::PrepareCast(AMobaBaseCharacter* Character, const FGameplayEventData* TriggerEventData)
{
	return Character != nullptr;
}

void UMobaGameplayAbility::OnCastStarted(AMobaBaseCharacter* Character)
{
}

void UMobaGameplayAbility::OnCastNotify(FGameplayEventData Payload)
{
}

void UMobaGameplayAbility::OnCastMontageDone()
{
}

void UMobaGameplayAbility::AdoptLegacyMontage(UAnimMontage* Legacy)
{
	if (!CastMontage && Legacy)
	{
		CastMontage = Legacy;
	}
}

void UMobaGameplayAbility::StartCastMontage()
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	UAnimMontage* Montage = GetCastMontage();

	const FGameplayTag NotifyTag = ResolveNotifyTag(Character);
	if (NotifyTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* WaitNotify = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			NotifyTag,
			nullptr,
			true);
		WaitNotify->EventReceived.AddDynamic(this, &UMobaGameplayAbility::HandleCastNotify);
		WaitNotify->ReadyForActivation();
	}

	if (Montage)
	{
		// Listen host plays the montage twice (local + replicated). BlendOut/Interrupted still go through here, so we gate with bCastMontageDone.
		UAbilityTask_PlayMontageAndWait* PlayMontage = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			Montage,
			1.f,
			NAME_None,
			true,
			0.f);
		PlayMontage->OnCompleted.AddDynamic(this, &UMobaGameplayAbility::HandleCastMontageDone);
		PlayMontage->OnBlendOut.AddDynamic(this, &UMobaGameplayAbility::HandleCastMontageDone);
		PlayMontage->OnInterrupted.AddDynamic(this, &UMobaGameplayAbility::HandleCastMontageDone);
		PlayMontage->OnCancelled.AddDynamic(this, &UMobaGameplayAbility::HandleCastMontageDone);
		PlayMontage->ReadyForActivation();
		if (ShouldUseMontageFallback())
		{
			// Dedicated server often never ticks pose, so PlayMontageAndWait never completes and the spec stays active.
			ScheduleMontageFallback(Montage);
		}
		return;
	}

	// Instant abilities (no anim) still need the same notify/end path.
	HandleCastNotify(FGameplayEventData());
	HandleCastMontageDone();
}

bool UMobaGameplayAbility::ShouldUseMontageFallback() const
{
	if (IsRunningDedicatedServer())
	{
		return true;
	}
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	return !Mesh || !Mesh->GetAnimInstance();
}

void UMobaGameplayAbility::ScheduleMontageFallback(UAnimMontage* Montage)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		HandleCastNotifyFallback();
		HandleCastMontageDone();
		return;
	}

	float Length = Montage ? Montage->GetPlayLength() : 0.25f;
	Length = FMath::Max(Length, 0.15f);
	float NotifyTime = Length * 0.35f;
	if (Montage)
	{
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
		{
			const float Trigger = Event.GetTriggerTime();
			if (Trigger > 0.f)
			{
				NotifyTime = FMath::Clamp(Trigger, 0.05f, Length);
				break;
			}
		}
	}

	World->GetTimerManager().SetTimer(
		NotifyFallbackTimer,
		this,
		&UMobaGameplayAbility::HandleCastNotifyFallback,
		NotifyTime,
		false);
	World->GetTimerManager().SetTimer(
		MontageFallbackTimer,
		this,
		&UMobaGameplayAbility::HandleCastMontageDone,
		Length,
		false);
}

void UMobaGameplayAbility::HandleCastNotifyFallback()
{
	HandleCastNotify(FGameplayEventData());
}

void UMobaGameplayAbility::HandleCastNotify(FGameplayEventData Payload)
{
	if (bCastNotifyFired)
	{
		return;
	}
	bCastNotifyFired = true;
	OnCastNotify(Payload);
}

void UMobaGameplayAbility::HandleCastMontageDone()
{
	if (bCastMontageDone)
	{
		return;
	}
	bCastMontageDone = true;
	if (!bCastNotifyFired)
	{
		// Montage ended without the slot notify. Still apply the hit so a missing notify doesn't eat the cast.
		HandleCastNotify(FGameplayEventData());
	}
	OnCastMontageDone();
	if (bEndOnMontage)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UMobaGameplayAbility::EndCastPlant()
{
	if (!bPlantedThisCast)
	{
		return;
	}
	bPlantedThisCast = false;
	if (AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo()))
	{
		Character->EndPlantedAbility();
	}
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
		// Don't check a timer on `this`. CanActivate runs on the CDO, which has no world.
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
	const bool bHit = UMobaCombatLibrary::ApplyMobaDamage(Target, InDamage, Source);
	if (!bHit)
	{
		return false;
	}

	UMobaCombatLibrary::ApplyMobaEffects(Target, Source, Effects, EMobaEffectTarget::HitActor);
	if (bApplySelfEffects)
	{
		// Multi-hit abilities pass true only on the first target so lifesteal doesn't stack.
		UMobaCombatLibrary::ApplyMobaEffects(Target, Source, Effects, EMobaEffectTarget::Self);
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
		// Energy is a server number. Predicting spend here rubber-bands the bar under lag.
		Character->SpendEnergy(EnergyCost);
	}
}
