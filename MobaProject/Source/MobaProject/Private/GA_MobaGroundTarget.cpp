#include "GA_MobaGroundTarget.h"
#include "Abilities/GameplayAbilityTriggerType.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "MobaBaseCharacter.h"
#include "MobaGroundMarker.h"

UGA_MobaGroundTarget::UGA_MobaGroundTarget()
{
	CooldownTag = FGameplayTag::RequestGameplayTag(FName("Cooldown.GroundTarget"), false);
	Cooldown = 6.f;
	ActivateEventTag = FGameplayTag::RequestGameplayTag(FName("Event.GroundTarget"), false);
	bHoldToAim = true;
	AimRingClass = AMobaGroundMarker::StaticClass();
	BlastClass = AMobaGroundMarker::StaticClass();

	const FGameplayTag CastingTag = FGameplayTag::RequestGameplayTag(FName("State.GroundCasting"), false);
	ActivationOwnedTags.AddTag(CastingTag);
	ActivationBlockedTags.AddTag(CastingTag);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = ActivateEventTag;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UGA_MobaGroundTarget::ActivateAbility(
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

	PendingBlastLocation = LocationFromEvent(TriggerEventData, Character, MaxRange);
	bBlastedThisCast = false;
	bEndedThisCast = false;
	ApplyMobaCooldown();

	if (!GroundTargetMontage)
	{
		DoBlast();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	Character->BeginPlantedAbility();

	UAbilityTask_WaitGameplayEvent* WaitBlast = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		FGameplayTag::RequestGameplayTag(FName("Event.GroundTarget.Blast"), false),
		nullptr,
		true,
		true);
	WaitBlast->EventReceived.AddDynamic(this, &UGA_MobaGroundTarget::OnBlastNotify);
	WaitBlast->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* PlayMontage = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		GroundTargetMontage,
		1.f,
		NAME_None,
		true,
		0.f);
	PlayMontage->OnCompleted.AddDynamic(this, &UGA_MobaGroundTarget::OnMontageDone);
	PlayMontage->OnBlendOut.AddDynamic(this, &UGA_MobaGroundTarget::OnMontageDone);
	PlayMontage->OnInterrupted.AddDynamic(this, &UGA_MobaGroundTarget::OnMontageDone);
	PlayMontage->OnCancelled.AddDynamic(this, &UGA_MobaGroundTarget::OnMontageDone);
	PlayMontage->ReadyForActivation();
}

void UGA_MobaGroundTarget::OnBlastNotify(FGameplayEventData Payload)
{
	DoBlast();
}

void UGA_MobaGroundTarget::DoBlast()
{
	if (bBlastedThisCast)
	{
		return;
	}
	bBlastedThisCast = true;

	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	if (CurrentActorInfo && CurrentActorInfo->IsLocallyControlled())
	{
		SpawnBlast(Character, PendingBlastLocation, true);
	}
	if (HasAuthority(&CurrentActivationInfo))
	{
		ApplyBlastDamage(Character, PendingBlastLocation);
		SpawnBlast(Character, PendingBlastLocation, false);
	}
}

void UGA_MobaGroundTarget::OnMontageDone()
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

void UGA_MobaGroundTarget::BeginHold(AMobaBaseCharacter* Avatar) const
{
	if (!Avatar || !Avatar->IsLocallyControlled())
	{
		return;
	}

	Avatar->ClearAimRing();

	UWorld* World = Avatar->GetWorld();
	if (!World)
	{
		return;
	}

	TSubclassOf<AMobaGroundMarker> RingClass = AimRingClass
		? AimRingClass
		: TSubclassOf<AMobaGroundMarker>(AMobaGroundMarker::StaticClass());

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = Avatar;
	Params.Instigator = Avatar;

	AMobaGroundMarker* Ring = World->SpawnActor<AMobaGroundMarker>(
		RingClass,
		TraceGroundAim(Avatar, MaxRange),
		FRotator::ZeroRotator,
		Params);
	if (Ring)
	{
		Ring->InitAsAimRing(Radius, MaxRange);
		Avatar->SetAimRing(Ring);
	}
}

void UGA_MobaGroundTarget::ConfirmHold(AMobaBaseCharacter* Avatar) const
{
	if (!Avatar || !Avatar->IsLocallyControlled() || !Avatar->GetAbilitySystemComponent())
	{
		return;
	}

	const FVector Location = Avatar->GetAimRing()
		? Avatar->GetAimRing()->GetActorLocation()
		: TraceGroundAim(Avatar, MaxRange);

	Avatar->ClearAimRing();

	FGameplayEventData EventData;
	EventData.EventTag = ActivateEventTag;
	EventData.Instigator = Avatar;
	EventData.Target = Avatar;
	EventData.TargetData = MakeLocationTargetData(Location);
	Avatar->GetAbilitySystemComponent()->HandleGameplayEvent(ActivateEventTag, &EventData);
}

void UGA_MobaGroundTarget::CancelHold(AMobaBaseCharacter* Avatar) const
{
	if (Avatar)
	{
		Avatar->ClearAimRing();
	}
}

FVector UGA_MobaGroundTarget::TraceGroundAim(const ACharacter* Character, float MaxRange)
{
	if (!Character)
	{
		return FVector::ZeroVector;
	}

	const FVector HeroLoc = Character->GetActorLocation();
	FVector CamLoc = HeroLoc + FVector(0.f, 0.f, 80.f);
	FRotator CamRot = Character->GetControlRotation();
	if (const APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		PC->GetPlayerViewPoint(CamLoc, CamRot);
	}

	const FVector CamDir = CamRot.Vector();
	const FVector TraceEnd = CamLoc + CamDir * 20000.f;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MobaGroundAim), false, Character);
	const bool bHit = Character->GetWorld() && Character->GetWorld()->LineTraceSingleByChannel(
		Hit,
		CamLoc,
		TraceEnd,
		ECC_WorldStatic,
		Params);

	FVector Point;
	if (bHit)
	{
		Point = Hit.ImpactPoint;
	}
	else if (FMath::Abs(CamDir.Z) > KINDA_SMALL_NUMBER)
	{
		const float T = (HeroLoc.Z - CamLoc.Z) / CamDir.Z;
		Point = T > 0.f ? (CamLoc + CamDir * T) : (HeroLoc + FVector(CamDir.X, CamDir.Y, 0.f).GetSafeNormal() * MaxRange);
		Point.Z = HeroLoc.Z;
	}
	else
	{
		Point = HeroLoc + FVector(CamDir.X, CamDir.Y, 0.f).GetSafeNormal() * MaxRange;
		Point.Z = HeroLoc.Z;
	}

	FVector Flat = Point - HeroLoc;
	Flat.Z = 0.f;
	if (Flat.SizeSquared() > FMath::Square(MaxRange))
	{
		Point = HeroLoc + Flat.GetSafeNormal() * MaxRange;
		Point.Z = bHit ? Hit.ImpactPoint.Z : HeroLoc.Z;
	}

	return Point;
}

FVector UGA_MobaGroundTarget::LocationFromEvent(
	const FGameplayEventData* EventData,
	const ACharacter* FallbackCharacter,
	float MaxRange)
{
	if (EventData && EventData->TargetData.Num() > 0)
	{
		if (const FGameplayAbilityTargetData* Data = EventData->TargetData.Get(0))
		{
			const FVector Point = Data->GetEndPoint();
			if (!Point.IsNearlyZero())
			{
				return Point;
			}
		}
	}

	return TraceGroundAim(FallbackCharacter, MaxRange);
}

void UGA_MobaGroundTarget::ApplyBlastDamage(ACharacter* Character, const FVector& Location) const
{
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MobaGroundBlast), false, Character);
	World->OverlapMultiByChannel(
		Overlaps,
		Location + FVector(0.f, 0.f, 80.f),
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(Radius),
		Params);

	TSet<AActor*> Damaged;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!IsValid(Target) || Target == Character || Damaged.Contains(Target))
		{
			continue;
		}
		if (AMobaBaseCharacter::ApplyMobaDamage(Target, Damage, Character))
		{
			Damaged.Add(Target);
		}
	}
}

void UGA_MobaGroundTarget::SpawnBlast(ACharacter* Character, const FVector& Location, bool bCosmetic) const
{
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	TSubclassOf<AMobaGroundMarker> ClassToSpawn = BlastClass
		? BlastClass
		: TSubclassOf<AMobaGroundMarker>(AMobaGroundMarker::StaticClass());

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = Character;
	Params.Instigator = Character;

	if (AMobaGroundMarker* Blast = World->SpawnActor<AMobaGroundMarker>(ClassToSpawn, Location, FRotator::ZeroRotator, Params))
	{
		Blast->InitAsBlast(Radius, BlastLifetime, bCosmetic);
	}
}
