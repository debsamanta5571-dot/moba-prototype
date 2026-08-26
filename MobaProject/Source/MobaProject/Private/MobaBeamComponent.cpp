#include "MobaBeamComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "MobaBaseCharacter.h"
#include "MobaHeroFxComponent.h"
#include "Net/UnrealNetwork.h"

UMobaBeamComponent::UMobaBeamComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void UMobaBeamComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UMobaBeamComponent, bRepBeaming, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UMobaBeamComponent, RepStart, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UMobaBeamComponent, RepDir, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UMobaBeamComponent, RepRange, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UMobaBeamComponent, RepRadius, COND_SkipOwner);
}

AMobaBaseCharacter* UMobaBeamComponent::GetHero() const
{
	return Cast<AMobaBaseCharacter>(GetOwner());
}

UMobaHeroFxComponent* UMobaBeamComponent::GetFx() const
{
	AMobaBaseCharacter* Hero = GetHero();
	return Hero ? Hero->GetHeroFx() : nullptr;
}

void UMobaBeamComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickBeam(DeltaTime);
}

void UMobaBeamComponent::Start(FName InSocket, const FVector& InOffset, float InRange, float InRadius, float Lifetime, float InTurnSpeed, float InMaxPitch)
{
	AMobaBaseCharacter* Hero = GetHero();
	if (!Hero)
	{
		return;
	}

	bBeamActive = true;
	bSmoothed = false;
	Socket = InSocket;
	Offset = InOffset;
	Range = FMath::Max(InRange, 50.f);
	Radius = FMath::Max(InRadius, 8.f);
	TurnSpeed = FMath::Max(InTurnSpeed, 1.f);
	MaxPitch = FMath::Clamp(InMaxPitch, 1.f, 85.f);
	EndTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f) + FMath::Max(Lifetime, 0.05f);
	const FRotator Aim = GetAimRotator();
	SmoothDir = Aim.Vector();
	SmoothStart = ComputeFirePoint(Aim);
	SmoothEnd = ClipEnd(SmoothStart, SmoothStart + SmoothDir * Range);
	if (Hero->HasAuthority())
	{
		bRepBeaming = true;
		RepStart = SmoothStart;
		RepDir = SmoothDir;
		RepRange = Range;
		RepRadius = Radius;
	}
	if (UMobaHeroFxComponent* Fx = GetFx())
	{
		Fx->PlayBeamDebug(SmoothStart, SmoothEnd, Radius, 0.2f);
	}
}

void UMobaBeamComponent::Stop()
{
	bBeamActive = false;
	bSmoothed = false;
	AMobaBaseCharacter* Hero = GetHero();
	if (Hero && Hero->HasAuthority())
	{
		bRepBeaming = false;
	}
	if (UMobaHeroFxComponent* Fx = GetFx())
	{
		Fx->StopBeamDebug();
	}
}

FRotator UMobaBeamComponent::GetAimRotator() const
{
	AMobaBaseCharacter* Hero = GetHero();
	if (!Hero)
	{
		return FRotator::ZeroRotator;
	}
	FRotator Aim = Hero->GetBaseAimRotation();
	Aim.Pitch = FMath::ClampAngle(Aim.Pitch, -MaxPitch, MaxPitch);
	Aim.Roll = 0.f;
	return Aim;
}

FVector UMobaBeamComponent::ComputeFirePoint(const FRotator& Aim) const
{
	AMobaBaseCharacter* Hero = GetHero();
	if (!Hero)
	{
		return FVector::ZeroVector;
	}

	FTransform Fire;
	if (Hero->FindMobaFirePoint(Socket, Fire))
	{
		return Fire.TransformPosition(Offset);
	}

	FVector UseOffset = Offset;
	if (UseOffset.IsNearlyZero())
	{
		UseOffset = FVector(70.f, 0.f, 40.f);
	}
	return Hero->GetActorLocation() + Aim.RotateVector(UseOffset);
}

FVector UMobaBeamComponent::ClipEnd(const FVector& Start, const FVector& WantedEnd) const
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World)
	{
		return WantedEnd;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MobaBeamClip), false, Owner);
	FHitResult Block;
	if (!World->LineTraceSingleByChannel(Block, Start, WantedEnd, ECC_Visibility, Params))
	{
		return WantedEnd;
	}
	if (Block.GetActor() && Block.GetActor()->IsA<APawn>())
	{
		return WantedEnd;
	}
	return Block.ImpactPoint;
}

void UMobaBeamComponent::TickBeam(float DeltaSeconds)
{
	AMobaBaseCharacter* Hero = GetHero();
	if (!Hero)
	{
		return;
	}

	const bool bDrive = bBeamActive && (Hero->IsLocallyControlled() || Hero->HasAuthority());
	const bool bFollowRep = !bDrive && bRepBeaming && !Hero->IsLocallyControlled();
	if (!bDrive && !bFollowRep)
	{
		if (!bBeamActive && !bRepBeaming && !Hero->IsLocallyControlled())
		{
			if (UMobaHeroFxComponent* Fx = GetFx())
			{
				Fx->StopBeamDebug();
			}
		}
		return;
	}

	if (bBeamActive && (Hero->IsDead() || Hero->IsStunned() || (GetWorld() && GetWorld()->GetTimeSeconds() >= EndTime)))
	{
		Stop();
		return;
	}

	FVector DesiredStart = SmoothStart;
	FVector DesiredDir = SmoothDir;
	float UseRange = Range;
	float UseRadius = Radius;
	if (bDrive)
	{
		const FRotator Aim = GetAimRotator();
		DesiredDir = Aim.Vector();
		DesiredStart = ComputeFirePoint(Aim);
	}
	else
	{
		DesiredStart = RepStart;
		DesiredDir = RepDir.GetSafeNormal();
		if (DesiredDir.IsNearlyZero())
		{
			DesiredDir = FVector::ForwardVector;
		}
		UseRange = RepRange;
		UseRadius = RepRadius;
	}

	if (!bSmoothed)
	{
		SmoothStart = DesiredStart;
		SmoothDir = DesiredDir;
		bSmoothed = true;
	}
	else
	{
		SmoothStart = FMath::VInterpTo(SmoothStart, DesiredStart, DeltaSeconds, FMath::Max(TurnSpeed + 4.f, 8.f));
		const FRotator Smoothed = FMath::RInterpTo(SmoothDir.Rotation(), DesiredDir.Rotation(), DeltaSeconds, TurnSpeed);
		SmoothDir = Smoothed.Vector();
	}

	SmoothEnd = ClipEnd(SmoothStart, SmoothStart + SmoothDir * FMath::Max(UseRange, 50.f));
	if (Hero->HasAuthority() && bBeamActive)
	{
		RepStart = SmoothStart;
		RepDir = SmoothDir;
		RepRange = UseRange;
		RepRadius = UseRadius;
		bRepBeaming = true;
	}
	if (UMobaHeroFxComponent* Fx = GetFx())
	{
		Fx->PlayBeamDebug(SmoothStart, SmoothEnd, UseRadius, 0.2f);
	}
}
