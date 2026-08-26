#include "GA_MobaTrace.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "MobaBaseCharacter.h"

UGA_MobaTrace::UGA_MobaTrace()
{
	Cooldown = 1.f;
	EnergyCost = 10.f;
	DefaultCastSfx = EMobaSfx::MeleeCast;
	DefaultHitSfx = EMobaSfx::MeleeHit;
}

void UGA_MobaTrace::PostLoad()
{
	Super::PostLoad();
	AdoptLegacyMontage(MeleeMontage); // old BPs still have MeleeMontage filled
}

void UGA_MobaTrace::OnCastStarted(AMobaBaseCharacter* Character)
{
	bHitThisSwing = false;
	ShowRangeRing();
}

void UGA_MobaTrace::OnCastNotify(FGameplayEventData Payload)
{
	TryHit();
}

void UGA_MobaTrace::ShowRangeRing()
{
	UAnimMontage* Montage = GetCastMontage();
	const bool bFireMontage = Montage && Montage->GetName().Contains(TEXT("Fire"));
	if (!bShowRangeRing && !bFireMontage)
	{
		return;
	}

	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	const float RingRadius = FMath::Max(Range + Radius, 40.f);
	const float Lifetime = FMath::Max(RangeRingLifetime, 0.15f);
	Character->PlayFireRingDebug(RingRadius, Lifetime);
	if (HasAuthority(&CurrentActivationInfo))
	{
		Character->MulticastFireRingVfx(RingRadius, Lifetime);
	}
}

void UGA_MobaTrace::TryHit()
{
	if (bHitThisSwing)
	{
		return;
	}
	bHitThisSwing = true;

	AMobaBaseCharacter* Character = Cast<AMobaBaseCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	PlayHitSfx(Character->GetActorLocation() + Character->GetActorForwardVector() * 80.f);

	// Predicted activate already played the swing. Only the server writes Health.
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	// Flatten pitch so looking up doesn't throw the sweep into the sky.
	const FRotator Yaw(0.f, Character->GetControlRotation().Yaw, 0.f);
	const FVector Start = Character->GetActorLocation();
	const FVector End = Start + Yaw.Vector() * Range;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Character);

	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);

	TArray<FHitResult> Hits;
	Character->GetWorld()->SweepMultiByObjectType(
		Hits,
		Start,
		End,
		FQuat::Identity,
		Objects,
		FCollisionShape::MakeSphere(Radius),
		Params);

	Hits.Sort([](const FHitResult& A, const FHitResult& B)
	{
		return A.Distance < B.Distance;
	});

	TSet<AActor*> Damaged;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!IsValid(Target) || Target == Character || Damaged.Contains(Target))
		{
			continue;
		}
		if (ApplyAbilityHit(Target, Damage, Damaged.Num() == 0)) // self-effects once, on the closest hit
		{
			Damaged.Add(Target);
			if (Damaged.Num() >= FMath::Max(1, MaxTargets))
			{
				break;
			}
		}
	}
}
