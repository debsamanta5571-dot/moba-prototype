#include "GA_MobaProjectile.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "MobaBaseCharacter.h"
#include "MobaProjectile.h"

UGA_MobaProjectile::UGA_MobaProjectile()
{
	Cooldown = 1.5f;
	EnergyCost = 25.f;
	DefaultCastSfx = EMobaSfx::SkillshotFire;
	bPlayCastSfxOnStart = false;
}

void UGA_MobaProjectile::PostLoad()
{
	Super::PostLoad();
	AdoptLegacyMontage(SkillshotMontage);
}

bool UGA_MobaProjectile::PrepareCast(AMobaBaseCharacter* Character, const FGameplayEventData* TriggerEventData)
{
	bFiredThisCast = false;
	return Character != nullptr && ProjectileClass != nullptr;
}

void UGA_MobaProjectile::OnCastNotify(FGameplayEventData Payload)
{
	FireShot();
}

void UGA_MobaProjectile::FireShot()
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
		// Listen host already spawned the authority bolt. Owning client only needs the fake one.
		SpawnBolt(true, false);
	}
}

void UGA_MobaProjectile::SpawnBolt(bool bCosmetic, bool bHideVisuals)
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !ProjectileClass)
	{
		return;
	}

	const FVector Dir = Character->GetControlRotation().Vector(); // camera, not actor yaw — orbs can lob
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

FVector UGA_MobaProjectile::GetSpawnLocation(AMobaBaseCharacter* Character) const
{
	if (!Character)
	{
		return FVector::ZeroVector;
	}

	FTransform Fire;
	if (Character->FindMobaFirePoint(SpawnSocket, Fire))
	{
		return Fire.TransformPosition(SpawnOffset);
	}

	const FRotator Yaw(0.f, Character->GetControlRotation().Yaw, 0.f);
	FVector Offset = SpawnOffset;
	if (Offset.IsNearlyZero())
	{
		Offset = FVector(80.f, 0.f, 40.f);
	}
	return Character->GetActorLocation() + Yaw.RotateVector(Offset);
}
