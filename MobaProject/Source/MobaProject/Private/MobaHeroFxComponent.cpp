#include "MobaHeroFxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GA_MobaGroundAoE.h"
#include "GameFramework/Character.h"
#include "MobaBaseCharacter.h"
#include "MobaDamageNumber.h"
#include "Components/CapsuleComponent.h"

UMobaHeroFxComponent::UMobaHeroFxComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UMobaHeroFxComponent::TickFx()
{
	UWorld* World = GetWorld();
	AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(GetOwner());
	if (!World || !Hero)
	{
		return;
	}

	if (Hero->IsLocallyControlled())
	{
		if (bGroundAiming && GroundBlastStartTime <= 0.f)
		{
			const FVector Loc = UGA_MobaGroundAoE::TraceGroundAim(Hero, GroundAimMaxRange);
			DrawDebugSphere(World, Loc, GroundAimRadius, 24, FColor::Cyan, false, -1.f, 0, 4.f);
		}
	}

	if (GroundBlastStartTime > 0.f)
	{
		const float Elapsed = World->GetTimeSeconds() - GroundBlastStartTime;
		if (Elapsed >= GroundBlastDuration)
		{
			GroundBlastStartTime = -100.f;
		}
		else
		{
			const float T = FMath::Clamp(Elapsed / FMath::Max(GroundBlastDuration, 0.2f), 0.f, 1.f);
			const float Grow = FMath::InterpEaseOut(0.08f, 1.f, T, 2.f);
			const float R = GroundBlastRadius * Grow;
			const FVector X(1.f, 0.f, 0.f);
			const FVector Y(0.f, 1.f, 0.f);
			DrawDebugSphere(World, GroundBlastLoc, R, 24, FColor::Red, false, -1.f, 0, 4.f);
			DrawDebugCircle(World, GroundBlastLoc + FVector(0.f, 0.f, 6.f), R, 48, FColor::Red, false, -1.f, 0, 4.f, X, Y, false);
		}
	}

	if (FireRingStartTime > 0.f)
	{
		const float Elapsed = World->GetTimeSeconds() - FireRingStartTime;
		if (Elapsed >= FireRingDuration)
		{
			FireRingStartTime = -100.f;
		}
		else
		{
			const float Alpha = 1.f - FMath::Clamp(Elapsed / FMath::Max(FireRingDuration, 0.05f), 0.f, 1.f);
			const float Grow = FMath::Clamp(Elapsed / 0.12f, 0.f, 1.f);
			const float R = FireRingRadius * FMath::Lerp(0.62f, 1.f, Grow);
			const float HalfH = Hero->GetCapsuleComponent() ? Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 96.f;
			const FVector Center = Hero->GetActorLocation() - FVector(0.f, 0.f, HalfH - 6.f);
			const FVector X(1.f, 0.f, 0.f);
			const FVector Y(0.f, 1.f, 0.f);
			const uint8 Fade = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(255.f * Alpha), 40, 255));
			DrawDebugCircle(World, Center, R, 48, FColor(255, 90, 16, Fade), false, -1.f, 0, 6.f, X, Y, false);
			DrawDebugCircle(World, Center + FVector(0.f, 0.f, 10.f), R * 0.92f, 40, FColor(255, 170, 32, Fade), false, -1.f, 0, 3.f, X, Y, false);
			DrawDebugCircle(World, Center + FVector(0.f, 0.f, 22.f), R * 0.84f, 32, FColor(255, 48, 8, Fade), false, -1.f, 0, 2.f, X, Y, false);
			const int32 Flames = 12;
			for (int32 i = 0; i < Flames; ++i)
			{
				const float Rad = (2.f * PI * static_cast<float>(i)) / static_cast<float>(Flames);
				const FVector Dir(FMath::Cos(Rad), FMath::Sin(Rad), 0.f);
				const FVector Inner = Center + Dir * (R * 0.88f);
				const FVector Outer = Center + Dir * (R * 1.06f) + FVector(0.f, 0.f, 28.f + 10.f * FMath::Sin(Elapsed * 18.f + Rad));
				DrawDebugLine(World, Inner, Outer, FColor(255, 110, 20, Fade), false, -1.f, 0, 2.5f);
			}
		}
	}

	if (BeamStartTime > 0.f)
	{
		const float Elapsed = World->GetTimeSeconds() - BeamStartTime;
		if (Elapsed >= BeamDuration)
		{
			BeamStartTime = -100.f;
		}
		else
		{
			const float InnerR = FMath::Max(BeamRadius * 0.28f, 6.f);
			DrawDebugCylinder(World, BeamStartLoc, BeamEndLoc, BeamRadius, 12, FColor(30, 160, 255), false, -1.f, 0, 1.5f);
			DrawDebugCylinder(World, BeamStartLoc, BeamEndLoc, InnerR, 8, FColor(210, 245, 255), false, -1.f, 0, 2.f);
			DrawDebugLine(World, BeamStartLoc, BeamEndLoc, FColor(160, 230, 255), false, -1.f, 0, 2.f);
			DrawDebugSphere(World, BeamEndLoc, FMath::Max(BeamRadius * 0.45f, 8.f), 10, FColor(180, 230, 255), false, -1.f, 0, 1.f);
		}
	}
}

void UMobaHeroFxComponent::StartGroundAimDebug(float Radius, float MaxRange)
{
	bGroundAiming = true;
	GroundAimRadius = Radius;
	GroundAimMaxRange = MaxRange;
}

void UMobaHeroFxComponent::StopGroundAimDebug()
{
	bGroundAiming = false;
}

void UMobaHeroFxComponent::PlayGroundBlastDebug(FVector Location, float Radius, float Lifetime)
{
	StopGroundAimDebug();
	AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(GetOwner());
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (GroundBlastStartTime > 0.f && (Now - GroundBlastStartTime) < GroundBlastDuration)
	{
		if (FVector::DistSquared(GroundBlastLoc, Location) <= 100.f)
		{
			return;
		}
		if (Hero && Hero->IsLocallyControlled())
		{
			return;
		}
	}
	GroundBlastLoc = Location;
	GroundBlastRadius = FMath::Max(Radius, 80.f);
	GroundBlastDuration = FMath::Max(Lifetime, 0.2f);
	GroundBlastStartTime = Now;
}

void UMobaHeroFxComponent::PlayFireRingDebug(float Radius, float Lifetime)
{
	FireRingRadius = FMath::Max(Radius, 20.f);
	FireRingDuration = FMath::Max(Lifetime, 0.05f);
	FireRingStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void UMobaHeroFxComponent::PlayBeamDebug(FVector Start, FVector End, float Radius, float Lifetime)
{
	BeamStartLoc = Start;
	BeamEndLoc = End;
	BeamRadius = FMath::Max(Radius, 8.f);
	BeamDuration = FMath::Max(Lifetime, 0.05f);
	BeamStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void UMobaHeroFxComponent::StopBeamDebug()
{
	BeamStartTime = -100.f;
}

void UMobaHeroFxComponent::SpawnFloatingNumber(FVector Location, float Amount, bool bGold)
{
	UWorld* World = GetWorld();
	if (!World || Amount <= 0.f)
	{
		return;
	}

	Location += FVector(FMath::FRandRange(-18.f, 18.f), FMath::FRandRange(-18.f, 18.f), FMath::FRandRange(-8.f, 16.f));
	if (bGold)
	{
		Location.Z += 36.f;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = GetOwner();
	Params.ObjectFlags |= RF_Transient;

	if (AMobaDamageNumber* Number = World->SpawnActor<AMobaDamageNumber>(
		AMobaDamageNumber::StaticClass(),
		Location,
		FRotator::ZeroRotator,
		Params))
	{
		Number->Init(Amount, bGold);
	}
}
