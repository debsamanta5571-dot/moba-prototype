#include "GA_MobaGroundTarget.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTriggerType.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "AnimNotify_GroundTargetBlast.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "MobaBaseCharacter.h"
#include "MobaGroundMarker.h"
#include "TimerManager.h"

namespace
{
	float GroundBlastDelay(UAnimMontage* Montage)
	{
		if (Montage)
		{
			for (const FAnimNotifyEvent& Event : Montage->Notifies)
			{
				if (Event.Notify && Event.Notify->IsA(UAnimNotify_GroundTargetBlast::StaticClass()))
				{
					return FMath::Max(0.f, Event.GetTriggerTime());
				}
			}
		}
		return 0.35f;
	}
}

namespace
{
	FVector LocationFromEvent(const FGameplayEventData* EventData, const ACharacter* FallbackCharacter, float MaxRange)
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
		return UGA_MobaGroundTarget::TraceGroundAim(FallbackCharacter, MaxRange);
	}
}

UGA_MobaGroundTarget::UGA_MobaGroundTarget()
{
	CooldownTag = FGameplayTag::RequestGameplayTag(FName("Cooldown.GroundTarget"), false);
	Cooldown = 6.f;
	EnergyCost = 40.f;
	DefaultCastSfx = EMobaSfx::GroundBlast;
	ActivateEventTag = FGameplayTag::RequestGameplayTag(FName("Event.GroundTarget.Activate"), false);
	bHoldToAim = true;
	AimRingClass = AMobaGroundMarker::StaticClass();
	BlastClass = AMobaGroundMarker::StaticClass();

	AbilityTriggers.Reset();
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = ActivateEventTag;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);

	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	const FGameplayTag CastingTag = FGameplayTag::RequestGameplayTag(FName("State.GroundCasting"), false);
	ActivationOwnedTags.AddTag(CastingTag);
	ActivationBlockedTags.AddTag(CastingTag);
}

void UGA_MobaGroundTarget::PostInitProperties()
{
	Super::PostInitProperties();
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
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
	Character->BeginPlantedAbility(2.5f);

	if (GroundTargetMontage)
	{
		UAbilityTask_PlayMontageAndWait* PlayMontage = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			GroundTargetMontage);
		PlayMontage->OnCompleted.AddDynamic(this, &UGA_MobaGroundTarget::OnMontageDone);
		PlayMontage->OnBlendOut.AddDynamic(this, &UGA_MobaGroundTarget::OnMontageDone);
		PlayMontage->OnInterrupted.AddDynamic(this, &UGA_MobaGroundTarget::OnMontageDone);
		PlayMontage->OnCancelled.AddDynamic(this, &UGA_MobaGroundTarget::OnMontageDone);
		PlayMontage->ReadyForActivation();
	}

	if (UWorld* World = Character->GetWorld())
	{
		World->GetTimerManager().ClearTimer(BlastTimer);
		World->GetTimerManager().ClearTimer(CastFailsafeTimer);
		const float Delay = GroundTargetMontage ? GroundBlastDelay(GroundTargetMontage) : 0.f;
		if (Delay <= KINDA_SMALL_NUMBER)
		{
			FireBlast();
		}
		else
		{
			World->GetTimerManager().SetTimer(BlastTimer, this, &UGA_MobaGroundTarget::FireBlast, Delay, false);
		}
		World->GetTimerManager().SetTimer(
			CastFailsafeTimer,
			this,
			&UGA_MobaGroundTarget::OnMontageDone,
			2.5f,
			false);
	}
}

void UGA_MobaGroundTarget::FireBlast()
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character || bBlastedThisCast)
	{
		return;
	}
	bBlastedThisCast = true;

	PlayCastSfxAt(PendingBlastLocation);
	Character->PlayGroundBlastDebug(PendingBlastLocation, Radius, BlastLifetime);
	if (Character->HasAuthority())
	{
		ApplyBlastDamage();
	}
}

void UGA_MobaGroundTarget::SpawnBlast(bool bCosmetic)
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return;
	}

	AMobaGroundMarker::DestroyAllFor(World, Character);

	TSubclassOf<AMobaGroundMarker> ClassToSpawn = BlastClass
		? BlastClass
		: TSubclassOf<AMobaGroundMarker>(AMobaGroundMarker::StaticClass());

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Instigator = Character;

	AMobaGroundMarker* Blast = World->SpawnActor<AMobaGroundMarker>(
		ClassToSpawn,
		PendingBlastLocation,
		FRotator::ZeroRotator,
		Params);
	if (Blast)
	{
		Blast->InitAsBlast(Radius, BlastLifetime, bCosmetic);
	}
}

void UGA_MobaGroundTarget::ApplyBlastDamage()
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MobaGroundBlast), false, Character);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	World->OverlapMultiByObjectType(
		Overlaps,
		PendingBlastLocation + FVector(0.f, 0.f, 80.f),
		FQuat::Identity,
		Objects,
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
		if (ApplyAbilityHit(Target, Damage, Damaged.Num() == 0))
		{
			Damaged.Add(Target);
		}
	}
}

void UGA_MobaGroundTarget::OnMontageDone()
{
	if (bEndedThisCast)
	{
		return;
	}
	if (!bBlastedThisCast)
	{
		FireBlast();
	}
	bEndedThisCast = true;
	if (AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UWorld* World = Character->GetWorld())
		{
			World->GetTimerManager().ClearTimer(CastFailsafeTimer);
			World->GetTimerManager().ClearTimer(BlastTimer);
		}
		Character->EndPlantedAbility();
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_MobaGroundTarget::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UWorld* World = Character->GetWorld())
		{
			World->GetTimerManager().ClearTimer(CastFailsafeTimer);
			World->GetTimerManager().ClearTimer(BlastTimer);
		}
		if (!bEndedThisCast)
		{
			bEndedThisCast = true;
			Character->EndPlantedAbility();
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_MobaGroundTarget::BeginHold(AMobaBaseCharacter* Avatar) const
{
	if (!Avatar || !Avatar->IsLocallyControlled())
	{
		return;
	}

	Avatar->ClearAimRing();
	if (UWorld* World = Avatar->GetWorld())
	{
		AMobaGroundMarker::DestroyAllFor(World, Avatar);
	}
	Avatar->StartGroundAimDebug(Radius, MaxRange);
}

void UGA_MobaGroundTarget::ConfirmHold(AMobaBaseCharacter* Avatar) const
{
	if (!Avatar || !Avatar->IsLocallyControlled() || !Avatar->GetAbilitySystemComponent())
	{
		return;
	}

	if (!Avatar->HasEnergy(EnergyCost))
	{
		Avatar->NotifyNotEnoughEnergy();
		return;
	}

	const FVector Location = Avatar->GetAimRing()
		? Avatar->GetAimRing()->GetActorLocation()
		: TraceGroundAim(Avatar, MaxRange);

	Avatar->ClearAimRing();
	Avatar->StopGroundAimDebug();

	const FGameplayTag ActivateTag = FGameplayTag::RequestGameplayTag(FName("Event.GroundTarget.Activate"), false);
	FGameplayEventData EventData;
	EventData.EventTag = ActivateTag;
	EventData.Instigator = Avatar;
	EventData.Target = Avatar;
	EventData.TargetData = MakeLocationTargetData(Location);
	Avatar->GetAbilitySystemComponent()->HandleGameplayEvent(ActivateTag, &EventData);
	Avatar->StartCooldown(CooldownTag, Cooldown);

	if (!Avatar->HasAuthority())
	{
		Avatar->ServerConfirmGroundTarget(Location);
	}
}

void UGA_MobaGroundTarget::CancelHold(AMobaBaseCharacter* Avatar) const
{
	if (Avatar)
	{
		Avatar->ClearAimRing();
		Avatar->StopGroundAimDebug();
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
