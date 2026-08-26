#include "GA_MobaBeam.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/World.h"
#include "MobaBaseCharacter.h"
#include "TimerManager.h"

UGA_MobaBeam::UGA_MobaBeam()
{
	Cooldown = 7.f;
	EnergyCost = 35.f;
	DefaultCastSfx = EMobaSfx::SkillshotFire;
	bEndOnMontage = false; // beam lives on the timers, not the montage length
	PlantDuration = 1.2f;
	const FGameplayTag BeamingTag = FGameplayTag::RequestGameplayTag(FName("State.Beaming"), false);
	AbilityTags.AddTag(BeamingTag);
	ActivationBlockedTags.AddTag(BeamingTag);
	ActivationOwnedTags.AddTag(BeamingTag);
}

bool UGA_MobaBeam::PrepareCast(AMobaBaseCharacter* Character, const FGameplayEventData* TriggerEventData)
{
	bEndedThisCast = false;
	HitThisBeam.Reset();
	return Character != nullptr && Character->GetWorld() != nullptr;
}

void UGA_MobaBeam::OnCastNotify(FGameplayEventData Payload)
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!World)
	{
		FinishBeam();
		return;
	}

	Character->StartActiveBeam(SpawnSocket, SpawnOffset, Range, Radius, Duration, TurnSpeed, MaxAimPitch);
	const float Interval = FMath::Clamp(TickInterval, 0.05f, 0.5f);
	World->GetTimerManager().SetTimer(TickTimer, this, &UGA_MobaBeam::TickBeam, Interval, true);
	World->GetTimerManager().SetTimer(EndTimer, this, &UGA_MobaBeam::FinishBeam, FMath::Max(Duration, Interval), false);
	TickBeam();
}

void UGA_MobaBeam::TickBeam()
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character || Character->IsDead() || Character->IsStunned() || !Character->IsBeamActive())
	{
		FinishBeam();
		return;
	}

	if (HasAuthority(&CurrentActivationInfo))
	{
		ApplyTickDamage(Character, Character->GetActiveBeamStart(), Character->GetActiveBeamEnd());
	}
}

void UGA_MobaBeam::ApplyTickDamage(AMobaBaseCharacter* Character, const FVector& Start, const FVector& End)
{
	if (!Character || !Character->GetWorld())
	{
		return;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Character);

	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);

	TArray<FHitResult> Hits;
	Character->GetWorld()->SweepMultiByObjectType(
		Hits,
		Start,
		End,
		FQuat::Identity,
		Objects,
		FCollisionShape::MakeSphere(FMath::Max(Radius, 8.f)),
		Params);

	TSet<AActor*> TickHit;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!IsValid(Target) || Target == Character || TickHit.Contains(Target))
		{
			continue;
		}
		const bool bFirst = !HitThisBeam.Contains(Target); // self-effects once per beam, not every tick
		if (ApplyAbilityHit(Target, DamagePerTick, bFirst))
		{
			TickHit.Add(Target);
			HitThisBeam.Add(Target);
		}
	}
}

void UGA_MobaBeam::FinishBeam()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_MobaBeam::StopBeamTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimer);
		World->GetTimerManager().ClearTimer(EndTimer);
	}
}

void UGA_MobaBeam::StopBeamVfx(AMobaBaseCharacter* Character)
{
	if (Character)
	{
		Character->StopActiveBeam();
	}
}

void UGA_MobaBeam::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	StopBeamTimers();
	if (!bEndedThisCast)
	{
		bEndedThisCast = true;
		if (AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo()))
		{
			StopBeamVfx(Character);
		}
	}
	HitThisBeam.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
