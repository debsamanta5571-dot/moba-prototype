#include "GA_MobaGroundAoE.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
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
		return UGA_MobaGroundAoE::TraceGroundAim(FallbackCharacter, MaxRange);
	}
}

UGA_MobaGroundAoE::UGA_MobaGroundAoE()
{
	Cooldown = 6.f;
	EnergyCost = 40.f;
	DefaultCastSfx = EMobaSfx::GroundBlast;
	bHoldToAim = true;
	bPlayCastSfxOnStart = false;
	bEndOnMontage = false;
	bPlantOnCast = true;
	PlantDuration = 0.4f;
	Damage = 80.f;
	AimRingClass = AMobaGroundMarker::StaticClass();
	BlastClass = AMobaGroundMarker::StaticClass();

	AbilityTriggers.Reset();
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	const FGameplayTag CastingTag = FGameplayTag::RequestGameplayTag(FName("State.GroundCasting"), false);
	ActivationOwnedTags.AddTag(CastingTag);
	ActivationBlockedTags.AddTag(CastingTag);
}

void UGA_MobaGroundAoE::PostInitProperties()
{
	Super::PostInitProperties();
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bHoldToAim = true;
	bPlayCastSfxOnStart = false;
	bEndOnMontage = false;
	PlantDuration = 0.4f;
	if (FMath::IsNearlyEqual(Damage, 40.f))
	{
		Damage = 80.f; // old default leaked onto BPs; slam is 80
	}
	AbilityTriggers.Reset();
}

void UGA_MobaGroundAoE::PostLoad()
{
	Super::PostLoad();
	bHoldToAim = true;
	bPlayCastSfxOnStart = false;
	bEndOnMontage = false;
	PlantDuration = 0.4f;
	if (FMath::IsNearlyEqual(Damage, 40.f))
	{
		Damage = 80.f;
	}
	AbilityTriggers.Reset();
	AdoptLegacyMontage(GroundTargetMontage);
}

bool UGA_MobaGroundAoE::PrepareCast(AMobaBaseCharacter* Character, const FGameplayEventData* TriggerEventData)
{
	bBlastedThisCast = false;
	BlastLocationRetries = 0;
	PendingBlastLocation = LocationFromEvent(TriggerEventData, Character, MaxRange);
	return Character != nullptr;
}

void UGA_MobaGroundAoE::OnCastStarted(AMobaBaseCharacter* Character)
{
	(void)Character;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(BlastTimer, this, &UGA_MobaGroundAoE::FireBlast, 0.2f, false);
		World->GetTimerManager().SetTimer(EndTimer, this, &UGA_MobaGroundAoE::FinishCast, 1.2f, false);
	}
}

void UGA_MobaGroundAoE::OnCastNotify(FGameplayEventData Payload)
{
	(void)Payload;
	FireBlast();
}

void UGA_MobaGroundAoE::StartCastMontage()
{
	if (!GetCastMontage())
	{
		if (UAnimMontage* Ground = LoadObject<UAnimMontage>(
			nullptr,
			TEXT("/Game/Moba/Montages/Ground_Animmontage.Ground_Animmontage")))
		{
			CastMontage = Ground;
		}
	}

	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	UAnimMontage* Montage = GetCastMontage();
	if (Character->IsLocallyControlled())
	{
		if (!Character->PlayCastMontageIfNeeded(Montage, 1.f))
		{
			Character->PlaySlamMontage();
		}
	}
	if (Character->HasAuthority())
	{
		if (Montage)
		{
			Character->MulticastPlayCastMontage(Montage, 1.f);
		}
		else
		{
			Character->MulticastPlaySlam();
		}
	}
}

void UGA_MobaGroundAoE::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BlastTimer);
		World->GetTimerManager().ClearTimer(EndTimer);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_MobaGroundAoE::FinishCast()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGA_MobaGroundAoE::FireBlast()
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character || bBlastedThisCast)
	{
		return;
	}

	if (Character->HasCommittedGroundTarget())
	{
		PendingBlastLocation = Character->GetCommittedGroundTarget();
	}
	else if (Character->HasAuthority() && !Character->IsLocallyControlled() && BlastLocationRetries < 8)
	{
		// Dedicated/listen: notify can beat the replicated ground point. Wait a couple ticks.
		++BlastLocationRetries;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(BlastTimer, this, &UGA_MobaGroundAoE::FireBlast, 0.05f, false);
		}
		return;
	}

	bBlastedThisCast = true;
	bCastNotifyFired = true;

	PlayCastSfxAt(PendingBlastLocation);
	EndCastPlant();
	ExecuteBlast();
}

void UGA_MobaGroundAoE::ExecuteBlast()
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
		Character->NotifyGroundBlast(PendingBlastLocation, Radius, BlastLifetime);
		Character->MulticastGroundBlastVfx(PendingBlastLocation, Radius, BlastLifetime);
		StartBlastWave();
	}
}

void UGA_MobaGroundAoE::SpawnBlast(bool bCosmetic)
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

void UGA_MobaGroundAoE::StartBlastWave()
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !Character->HasAuthority())
	{
		return;
	}

	WaveOrigin = PendingBlastLocation;
	if (WaveOrigin.IsNearlyZero())
	{
		WaveOrigin = Character->GetActorLocation();
	}
	WaveMaxRadius = FMath::Max(Radius, 80.f);
	WaveDuration = FMath::Max(BlastLifetime, 0.2f);
	WaveDamage = Damage > 0.f ? Damage : 80.f;
	bWaveHitsTowers = bCanDamageTowers;
	WaveHit.Reset();
	WaveStartTime = Character->GetWorld() ? Character->GetWorld()->GetTimeSeconds() : 0.f;

	if (UWorld* World = Character->GetWorld())
	{
		World->GetTimerManager().SetTimer(
			WaveTimer,
			this,
			&UGA_MobaGroundAoE::TickBlastWave,
			0.05f,
			true);
	}
	TickBlastWave();
}

void UGA_MobaGroundAoE::StopBlastWave()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaveTimer);
	}
	WaveStartTime = -100.f;
	WaveHit.Reset();
}

void UGA_MobaGroundAoE::TickBlastWave()
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!World || !Character->HasAuthority() || WaveStartTime <= 0.f)
	{
		StopBlastWave();
		return;
	}

	const float Elapsed = World->GetTimeSeconds() - WaveStartTime;
	const float Total = FMath::Max(WaveDuration, 0.2f);
	if (Elapsed >= Total)
	{
		StopBlastWave();
		return;
	}

	const float T = FMath::Clamp(Elapsed / Total, 0.f, 1.f);
	const float Grow = FMath::InterpEaseOut(0.08f, 1.f, T, 2.f);
	const float CurrentR = WaveMaxRadius * Grow;

	TSet<AActor*> DamagedNow;
	auto TryHit = [this, Character, CurrentR, &DamagedNow](AActor* Target)
	{
		if (!IsValid(Target) || Target == Character || WaveHit.Contains(Target) || DamagedNow.Contains(Target))
		{
			return;
		}
		if (!bWaveHitsTowers && Cast<AMobaTower>(Target))
		{
			return;
		}
		FVector Delta = Target->GetActorLocation() - WaveOrigin;
		Delta.Z = 0.f;
		float Body = 0.f;
		if (const ACharacter* Other = Cast<ACharacter>(Target))
		{
			if (const UCapsuleComponent* Cap = Other->GetCapsuleComponent())
			{
				Body = Cap->GetScaledCapsuleRadius();
			}
		}
		const float Reach = CurrentR + Body;
		if (Delta.SizeSquared() > Reach * Reach)
		{
			return;
		}
		if (ApplyAbilityHit(Target, WaveDamage, WaveHit.Num() == 0))
		{
			DamagedNow.Add(Target);
			WaveHit.Add(Target);
		}
	};

	for (TActorIterator<AMobaBaseCharacter> It(World); It; ++It)
	{
		TryHit(*It);
	}
	for (TActorIterator<AMobaMinion> It(World); It; ++It)
	{
		TryHit(*It);
	}
	for (TActorIterator<AMobaTower> It(World); It; ++It)
	{
		TryHit(*It);
	}
}

void UGA_MobaGroundAoE::BeginHold(AMobaBaseCharacter* Avatar) const
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

void UGA_MobaGroundAoE::ConfirmHold(AMobaBaseCharacter* Avatar) const
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

// Camera line to the floor, clamped to MaxRange. Shared with the aim ring so confirm matches what you saw.

void UGA_MobaGroundAoE::CancelHold(AMobaBaseCharacter* Avatar) const
{
	if (Avatar)
	{
		Avatar->ClearAimRing();
		Avatar->StopGroundAimDebug();
	}
}

FVector UGA_MobaGroundAoE::TraceGroundAim(const ACharacter* Character, float MaxRange)
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
