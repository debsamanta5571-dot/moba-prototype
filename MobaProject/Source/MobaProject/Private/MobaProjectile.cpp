#include "MobaProjectile.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "MobaBaseCharacter.h"
#include "MobaSfx.h"

AMobaProjectile::AMobaProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = false;
	bOnlyRelevantToOwner = false;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(20.f);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AMobaProjectile::OnSphereBeginOverlap);
	Collision->OnComponentHit.AddDynamic(this, &AMobaProjectile::OnHit);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetIsReplicated(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = Collision;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 3000.f;
	ProjectileMovement->bInterpMovement = false;
	ProjectileMovement->bInterpRotation = false;
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &AMobaProjectile::OnStop);

	SetupCollision();
}

void AMobaProjectile::SetupCollision()
{
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetGenerateOverlapEvents(true);
	Collision->SetNotifyRigidBodyCollision(true);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCanEverAffectNavigation(false);

	ProjectileMovement->bSweepCollision = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bForceSubStepping = true;
}

void AMobaProjectile::InitFlight(
	const FVector& Direction,
	float Speed,
	float InDamage,
	float Lifetime,
	bool bInCosmetic,
	const TArray<FMobaEffectSpec>& InHitEffects,
	bool bInHideVisuals)
{
	Damage = InDamage;
	bCosmetic = bInCosmetic;
	bHideVisuals = bInHideVisuals;
	bConsumed = false;
	HitEffects = bInCosmetic ? TArray<FMobaEffectSpec>() : InHitEffects;
	SetupCollision();

	if (bCosmetic)
	{
		SetReplicates(false);
		SetReplicateMovement(false);
		SetActorHiddenInGame(false);
		ShowAllVisuals();
	}
	else
	{
		SetReplicates(!bHideVisuals);
		SetReplicateMovement(!bHideVisuals);
	}

	if (bHideVisuals)
	{
		HideAllVisuals();
	}

	if (AActor* Ignore = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(Ignore, true);
	}
	if (AActor* IgnoreOwner = GetOwner())
	{
		Collision->IgnoreActorWhenMoving(IgnoreOwner, true);
	}

	const FVector Dir = Direction.GetSafeNormal();
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->Velocity = Dir * Speed;

	if (Lifetime > 0.f)
	{
		SetLifeSpan(Lifetime);
	}

	if (!bCosmetic)
	{
		ForceNetUpdate();
	}
}

void AMobaProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (bHideVisuals)
	{
		HideAllVisuals();
	}
}

void AMobaProjectile::HideAllVisuals()
{
	bHideVisuals = true;
	TArray<UPrimitiveComponent*> Prims;
	GetComponents<UPrimitiveComponent>(Prims);
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (Prim && Prim != Collision)
		{
			Prim->SetVisibility(false, true);
			Prim->SetHiddenInGame(true, true);
		}
	}
}

void AMobaProjectile::ShowAllVisuals()
{
	bHideVisuals = false;
	TArray<UPrimitiveComponent*> Prims;
	GetComponents<UPrimitiveComponent>(Prims);
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (Prim && Prim != Collision)
		{
			Prim->SetVisibility(true, true);
			Prim->SetHiddenInGame(false, true);
		}
	}
}

bool AMobaProjectile::IsNetRelevantFor(
	const AActor* RealViewer,
	const AActor* ViewTarget,
	const FVector& SrcLocation) const
{
	if (bCosmetic)
	{
		return false;
	}

	if (ViewTarget && (ViewTarget == GetOwner() || ViewTarget == GetInstigator()))
	{
		return false;
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner() ? GetOwner() : GetInstigator());
	if (OwnerPawn && RealViewer == OwnerPawn->GetController())
	{
		return false;
	}

	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

void AMobaProjectile::OnSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (bConsumed || OtherActor == GetInstigator() || OtherActor == GetOwner() || Cast<AMobaProjectile>(OtherActor))
	{
		return;
	}
	ConsumeAndDestroy(OtherActor);
}

void AMobaProjectile::OnHit(
	UPrimitiveComponent* HitComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (OtherActor == GetInstigator() || OtherActor == GetOwner() || Cast<AMobaProjectile>(OtherActor))
	{
		return;
	}
	ConsumeAndDestroy(OtherActor);
}

void AMobaProjectile::OnStop(const FHitResult& ImpactResult)
{
	AActor* HitActor = ImpactResult.GetActor();
	if (HitActor == GetInstigator() || HitActor == GetOwner() || Cast<AMobaProjectile>(HitActor))
	{
		return;
	}
	ConsumeAndDestroy(HitActor);
}

void AMobaProjectile::ConsumeAndDestroy(AActor* DamageTarget)
{
	if (bConsumed)
	{
		return;
	}

	bConsumed = true;
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (DamageTarget && !bCosmetic && HasAuthority())
	{
		AActor* Source = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();
		if (AMobaBaseCharacter::ApplyMobaDamage(DamageTarget, Damage, Source))
		{
			AMobaBaseCharacter::ApplyMobaEffects(DamageTarget, Source, HitEffects, EMobaEffectTarget::HitActor);
			AMobaBaseCharacter::ApplyMobaEffects(DamageTarget, Source, HitEffects, EMobaEffectTarget::Self);
		}
	}

	Destroy();
}

void AMobaProjectile::SpawnDestroyVfx()
{
	if (bHideVisuals || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const APawn* Shooter = GetInstigator();
	const bool bOwnerClient = Shooter && Shooter->IsLocallyControlled() && GetNetMode() == NM_Client;
	if (bOwnerClient && !bCosmetic)
	{
		return;
	}

	UMobaSfx::Play(this, DestroySound, EMobaSfx::ProjectileDestroy, GetActorLocation());

	if (!DestroyVfxClass || !GetWorld())
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Instigator = GetInstigator();
	AActor* Vfx = GetWorld()->SpawnActor<AActor>(
		DestroyVfxClass,
		GetActorLocation(),
		GetActorRotation(),
		Params);
	if (Vfx)
	{
		Vfx->SetActorEnableCollision(false);
		if (DestroyVfxLife > 0.f)
		{
			Vfx->SetLifeSpan(DestroyVfxLife);
		}
	}
}

void AMobaProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		SpawnDestroyVfx();
	}
	Super::EndPlay(EndPlayReason);
}
