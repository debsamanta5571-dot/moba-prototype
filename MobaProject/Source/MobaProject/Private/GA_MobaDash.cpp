#include "GA_MobaDash.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MobaBaseCharacter.h"

UGA_MobaDash::UGA_MobaDash()
{
	Cooldown = 3.f;
	EnergyCost = 20.f;
	DefaultCastSfx = EMobaSfx::Dash;
	bSendMoveDirection = true; // WASD this frame has to ride with the activate or the server dashes the wrong way
	bPlantOnCast = false;
	bEndOnMontage = false; // root motion outlives the montage, we EndAbility from OnDashFinished

	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dashing"), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dashing"), false));
}

void UGA_MobaDash::PostLoad()
{
	Super::PostLoad();
	AdoptLegacyMontage(DashMontage);
}

bool UGA_MobaDash::PrepareCast(AMobaBaseCharacter* Character, const FGameplayEventData* TriggerEventData)
{
	if (!Character)
	{
		return false;
	}
	if (Character->HasPendingAbilityDirection())
	{
		PendingDashDir = Character->ConsumePendingAbilityDirection();
	}
	else
	{
		PendingDashDir = DirectionFromEvent(TriggerEventData, Character);
	}
	return !PendingDashDir.IsNearlyZero();
}

void UGA_MobaDash::OnCastStarted(AMobaBaseCharacter* Character)
{
	if (!Character)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const float FinishSpeed = Character->GetCharacterMovement()
		? Character->GetCharacterMovement()->MaxWalkSpeed
		: 500.f;

	// Root motion goes through CMC so the SavedMove includes it. SetActorLocation on the client rubber-bands.
	UAbilityTask_ApplyRootMotionConstantForce* DashTask =
		UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this,
			FName(TEXT("MobaDash")),
			PendingDashDir,
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
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
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
			Dir.Z = 0.f; // we send a unit vector in the event, not a world point
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
