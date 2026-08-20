#include "MobaProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "MobaBaseCharacter.h"

AMobaProjectile::AMobaProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(20.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetNotifyRigidBodyCollision(true);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AMobaProjectile::OnSphereBeginOverlap);
	Collision->OnComponentHit.AddDynamic(this, &AMobaProjectile::OnHit);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = Collision;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 3000.f;
	ProjectileMovement->bInterpMovement = true;
	ProjectileMovement->bInterpRotation = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &AMobaProjectile::OnStop);
}

void AMobaProjectile::InitFlight(const FVector& Direction, float Speed, float InDamage, float Lifetime, bool bInCosmetic)
{
	Damage = InDamage;
	bCosmetic = bInCosmetic;

	if (bCosmetic)
	{
		SetReplicates(false);
		SetReplicateMovement(false);
	}
	else
	{
		SetReplicateMovement(true);
		Mesh->SetOwnerNoSee(true);
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

bool AMobaProjectile::IsNetRelevantFor(
	const AActor* RealViewer,
	const AActor* ViewTarget,
	const FVector& SrcLocation) const
{
	if (bCosmetic)
	{
		return false;
	}

	const APawn* InstigatorPawn = GetInstigator();
	if (InstigatorPawn && (ViewTarget == InstigatorPawn || RealViewer == InstigatorPawn->GetController()))
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
	AActor* Source = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();
	if (bConsumed || !IsValid(OtherActor) || OtherActor == Source || OtherActor == GetOwner())
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
	AActor* Source = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();
	if (OtherActor == Source || OtherActor == GetOwner())
	{
		return;
	}
	ConsumeAndDestroy(nullptr);
}

void AMobaProjectile::OnStop(const FHitResult& ImpactResult)
{
	ConsumeAndDestroy(nullptr);
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
		AMobaBaseCharacter::ApplyMobaDamage(DamageTarget, Damage, Source);
	}

	Destroy();
}
