#include "GA_MobaDash.h"
#include "Abilities/GameplayAbilityTriggerType.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MobaBaseCharacter.h"

UGA_MobaDash::UGA_MobaDash()
{
	Cooldown = 3.f;
	EnergyCost = 20.f;
	DefaultCastSfx = EMobaSfx::Dash;
	ActivateEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Dash"), false);
	bSendMoveDirection = true;

	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dashing"), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dashing"), false));

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = ActivateEventTag;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UGA_MobaDash::ActivateAbility(
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

	const FVector Direction = DirectionFromEvent(TriggerEventData, Character);
	if (Direction.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ApplyMobaCooldown();
	PlayCastSfx();

	if (DashMontage)
	{
		UAbilityTask_PlayMontageAndWait* PlayMontage = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			DashMontage,
			1.f,
			NAME_None,
			true,
			0.f);
		PlayMontage->ReadyForActivation();
	}

	const float FinishSpeed = Character->GetCharacterMovement()
		? Character->GetCharacterMovement()->MaxWalkSpeed
		: 500.f;

	UAbilityTask_ApplyRootMotionConstantForce* DashTask =
		UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this,
			FName(TEXT("MobaDash")),
			Direction,
			DashStrength,
			DashDuration,
			false,
			nullptr,
			ERootMotionFinishVelocityMode::ClampVelocity,
			FVector::ZeroVector,
			FinishSpeed,
			true);

	if (!DashTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	DashTask->OnFinish.AddDynamic(this, &UGA_MobaDash::OnDashFinished);
	DashTask->ReadyForActivation();
}

void UGA_MobaDash::OnDashFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

FVector UGA_MobaDash::DirectionFromEvent(const FGameplayEventData* EventData, const ACharacter* FallbackCharacter)
{
	if (EventData && EventData->TargetData.Num() > 0)
	{
		if (const FGameplayAbilityTargetData* Data = EventData->TargetData.Get(0))
		{
			FVector Dir = Data->GetEndPoint();
			Dir.Z = 0.f;
			Dir = Dir.GetSafeNormal();
			if (!Dir.IsNearlyZero())
			{
				return Dir;
			}
		}
	}

	if (const AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(FallbackCharacter))
	{
		return Hero->GetMoveDashDirection();
	}
	return FVector::ZeroVector;
}
