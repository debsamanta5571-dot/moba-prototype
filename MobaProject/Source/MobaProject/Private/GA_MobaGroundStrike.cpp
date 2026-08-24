#include "GA_MobaGroundStrike.h"
#include "Engine/World.h"
#include "MobaBaseCharacter.h"
#include "MobaProjectile.h"

UGA_MobaGroundStrike::UGA_MobaGroundStrike()
{
	Damage = 40.f;
	Radius = 180.f;
	DefaultCastSfx = EMobaSfx::SkillshotFire;
}

void UGA_MobaGroundStrike::ExecuteBlast()
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("GroundStrike: ProjectileClass is not set. Using ground blast damage."));
		Super::ExecuteBlast();
		return;
	}

	Character->StopGroundAimDebug();
	if (Character->IsLocallyControlled())
	{
		Character->PlayGroundBlastDebug(PendingBlastLocation, Radius, BlastLifetime);
	}

	const bool bLocal = Character->IsLocallyControlled();
	const bool bAuth = Character->HasAuthority();
	if (bAuth)
	{
		Character->MulticastGroundBlastVfx(PendingBlastLocation, Radius, BlastLifetime);
		const FVector Start = PendingBlastLocation + FVector(0.f, 0.f, FMath::Max(DropHeight, 200.f));
		Character->MulticastSkillshotVfx(
			ProjectileClass,
			Start,
			FVector::DownVector.Rotation(),
			FVector::DownVector,
			Speed,
			Lifetime,
			PendingBlastLocation.Z + 40.f);
		ApplyBlastDamage();
	}
	if (bLocal)
	{
		SpawnDrop(true, false);
	}
}

void UGA_MobaGroundStrike::SpawnDrop(bool bCosmetic, bool bHideVisuals)
{
	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !ProjectileClass)
	{
		return;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return;
	}

	const float Height = FMath::Max(DropHeight, 200.f);
	const FVector Start = PendingBlastLocation + FVector(0.f, 0.f, Height);
	const FVector Dir = FVector::DownVector;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Instigator = Character;
	Params.Owner = Character;

	AMobaProjectile* Bolt = World->SpawnActor<AMobaProjectile>(
		ProjectileClass,
		Start,
		Dir.Rotation(),
		Params);
	if (!Bolt)
	{
		UE_LOG(LogTemp, Warning, TEXT("GroundStrike: failed to spawn drop projectile."));
		return;
	}

	Bolt->InitFlight(
		Dir,
		Speed,
		Damage,
		Lifetime,
		bCosmetic,
		bCosmetic ? TArray<FMobaEffectSpec>() : Effects,
		bHideVisuals,
		bCanDamageTowers);
	Bolt->SetImpactRadius(Radius);
	Bolt->SetExplodeAtZ(PendingBlastLocation.Z + 40.f);
}
