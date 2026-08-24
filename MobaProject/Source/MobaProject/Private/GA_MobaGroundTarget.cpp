#include "GA_MobaGroundTarget.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTriggerType.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "MobaBaseCharacter.h"
#include "MobaGroundMarker.h"
#include "MobaMinion.h"
#include "MobaTower.h"
#include "TimerManager.h"

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
		if (AMobaBaseCharacter* Hero = const_cast<AMobaBaseCharacter*>(Cast<AMobaBaseCharacter>(FallbackCharacter)))
		{
			if (Hero->HasPendingAbilityLocation())
			{
				return Hero->ConsumePendingAbilityLocation();
			}
		}
		return UGA_MobaGroundTarget::TraceGroundAim(FallbackCharacter, MaxRange);
	}
}

UGA_MobaGroundTarget::UGA_MobaGroundTarget()
{
	Cooldown = 6.f;
	EnergyCost = 40.f;
	DefaultCastSfx = EMobaSfx::GroundBlast;
	ActivateEventTag = FGameplayTag::RequestGameplayTag(FName("Event.GroundTarget.Activate"), false);
	AnimNotifyTag = FGameplayTag::RequestGameplayTag(FName("Ability.3"), false);
	bHoldToAim = true;
	AimRingClass = AMobaGroundMarker::StaticClass();
	BlastClass = AMobaGroundMarker::StaticClass();

	AbilityTriggers.Reset();
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	const FGameplayTag CastingTag = FGameplayTag::RequestGameplayTag(FName("State.GroundCasting"), false);
	ActivationOwnedTags.AddTag(CastingTag);
	ActivationBlockedTags.AddTag(CastingTag);
}

void UGA_MobaGroundTarget::PostInitProperties()
{
	Super::PostInitProperties();
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bHoldToAim = true;
	AbilityTriggers.Reset();
}

void UGA_MobaGroundTarget::PostLoad()
{
	Super::PostLoad();
	bHoldToAim = true;
	AbilityTriggers.Reset();
}

void UGA_MobaGroundTarget::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Do not Super: BP children of this class have empty K2_ActivateAbility /
	// K2_ActivateAbilityFromEvent nodes. FromEvent with no payload ends the ability.

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

	TArray<FGameplayTag> NotifyTags;
	NotifyTags.Add(AnimNotifyTag);
	NotifyTags.Add(ResolveNotifyTag(Character));
	NotifyTags.Add(ResolveCooldownTag(Character));
	NotifyTags.Add(FGameplayTag::RequestGameplayTag(FName("Ability.3"), false));
	NotifyTags.Add(FGameplayTag::RequestGameplayTag(FName("Ability.4"), false));
	NotifyTags.Add(FGameplayTag::RequestGameplayTag(FName("Event.GroundTarget.Blast"), false));

	TSet<FGameplayTag> UniqueTags;
	for (const FGameplayTag& Tag : NotifyTags)
	{
		if (!Tag.IsValid() || UniqueTags.Contains(Tag))
		{
			continue;
		}
		UniqueTags.Add(Tag);
		UAbilityTask_WaitGameplayEvent* WaitNotify = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			Tag,
			nullptr,
			true);
		WaitNotify->EventReceived.AddDynamic(this, &UGA_MobaGroundTarget::OnAnimNotify);
		WaitNotify->ReadyForActivation();
	}

	if (UWorld* World = Character->GetWorld())
	{
		World->GetTimerManager().ClearTimer(CastFailsafeTimer);
		World->GetTimerManager().SetTimer(
			CastFailsafeTimer,
			this,
			&UGA_MobaGroundTarget::OnMontageDone,
			GroundTargetMontage ? 2.0f : 0.05f,
			false);
	}
}

void UGA_MobaGroundTarget::OnAnimNotify(FGameplayEventData Payload)
{
	FireBlast();
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
	ExecuteBlast();
}

void UGA_MobaGroundTarget::ExecuteBlast()
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	Character->StopGroundAimDebug();
	if (Character->IsLocallyControlled())
	{
		Character->PlayGroundBlastDebug(PendingBlastLocation, Radius, BlastLifetime);
	}
	if (Character->HasAuthority())
	{
		Character->MulticastGroundBlastVfx(PendingBlastLocation, Radius, BlastLifetime);
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
	if (!World || !Character->HasAuthority())
	{
		return;
	}

	FVector Origin = PendingBlastLocation;
	if (Origin.IsNearlyZero())
	{
		Origin = Character->GetActorLocation();
	}
	const float HitRadius = FMath::Max(Radius, 80.f);
	const float R2 = FMath::Square(HitRadius);
	const float DamageAmount = Damage > 0.f ? Damage : 40.f;

	TSet<AActor*> Damaged;
	auto TryHitActor = [this, Character, Origin, R2, DamageAmount, &Damaged](AActor* Target)
	{
		if (!IsValid(Target) || Target == Character || Damaged.Contains(Target))
		{
			return;
		}
		FVector Delta = Target->GetActorLocation() - Origin;
		Delta.Z = 0.f;
		if (Delta.SizeSquared() > R2)
		{
			return;
		}
		if (ApplyAbilityHit(Target, DamageAmount, Damaged.Num() == 0))
		{
			Damaged.Add(Target);
		}
	};

	for (TActorIterator<AMobaBaseCharacter> It(World); It; ++It)
	{
		TryHitActor(*It);
	}
	for (TActorIterator<AMobaMinion> It(World); It; ++It)
	{
		TryHitActor(*It);
	}
	for (TActorIterator<AMobaTower> It(World); It; ++It)
	{
		TryHitActor(*It);
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
	Avatar->SetPendingAbilityLocation(Location);

	TSubclassOf<UGameplayAbility> Class = GetClass();
	if (UAbilitySystemComponent* ASC = Avatar->GetAbilitySystemComponent())
	{
		if (!ASC->TryActivateAbilityByClass(Class))
		{
			const FGameplayTag CastingTag = FGameplayTag::RequestGameplayTag(FName("State.GroundCasting"), false);
			if (CastingTag.IsValid())
			{
				ASC->RemoveLooseGameplayTag(CastingTag);
			}
			ASC->TryActivateAbilityByClass(Class);
		}
	}

	if (!Avatar->HasAuthority())
	{
		Avatar->ServerConfirmGroundTarget(Class, Location);
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
