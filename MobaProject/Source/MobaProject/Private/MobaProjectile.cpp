#include "MobaProjectile.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"
#include "MobaTower.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MobaBaseCharacter.h"
#include "MobaSfx.h"

AMobaProjectile::AMobaProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = false;
	SetReplicateMovement(false);
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
	Mesh->SetCastShadow(false);
	Mesh->SetCanEverAffectNavigation(false);

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
	bool bInHideVisuals,
	bool bInCanDamageTowers)
{
	Damage = InDamage;
	bCosmetic = bInCosmetic;
	bHideVisuals = bInHideVisuals;
	bCanDamageTowers = bInCanDamageTowers;
	bConsumed = false;
	HomingTarget = nullptr;
	HitEffects = bInCosmetic ? TArray<FMobaEffectSpec>() : InHitEffects;
	SetupCollision();
	SetActorTickEnabled(false);
	ProjectileMovement->bIsHomingProjectile = false;
	ProjectileMovement->HomingTargetComponent = nullptr;
	ProjectileMovement->HomingAccelerationMagnitude = 0.f;
	ProjectileMovement->bSweepCollision = true;

	if (bCosmetic || bHideVisuals)
	{
		SetReplicates(false);
		SetReplicateMovement(false);
	}
	else
	{
		SetReplicates(true);
		SetReplicateMovement(true);
	}

	if (bCosmetic)
	{
		SetActorHiddenInGame(false);
		ShowAllVisuals();
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

	ApplyLook();
}

void AMobaProjectile::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLook();
}

void AMobaProjectile::ApplyLook()
{
	if (Collision)
	{
		Collision->SetSphereRadius(FMath::Max(CollisionRadius, 1.f), false);
	}

	if (!Mesh)
	{
		return;
	}

	UStaticMesh* Sphere = BoltMesh;
	if (!Sphere)
	{
		Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	}
	if (Sphere && Mesh->GetStaticMesh() != Sphere)
	{
		Mesh->SetStaticMesh(Sphere);
	}

	const float Scale = FMath::Max(VisualScale, 0.1f);
	Mesh->SetRelativeScale3D(FVector(Scale));
	Mesh->SetCastShadow(false);

	UMaterialInterface* Base = BoltMaterial;
	if (!Base)
	{
		Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Moba/Art/M_MobaBolt.M_MobaBolt"));
	}
	if (!Base)
	{
		return;
	}

	if (!BoltMid || BoltMid->Parent != Base)
	{
		BoltMid = UMaterialInstanceDynamic::Create(Base, this);
	}
	if (BoltMid)
	{
		BoltMid->SetVectorParameterValue(TEXT("BoltColor"), BoltColor);
		Mesh->SetMaterial(0, BoltMid);
	}
}

void AMobaProjectile::InitHoming(AActor* Target, float Speed, float InDamage, float Lifetime)
{
	const FVector ToTarget = IsValid(Target)
		? (Target->GetActorLocation() - GetActorLocation())
		: GetActorForwardVector();
	InitFlight(ToTarget, Speed, InDamage, FMath::Max(Lifetime, 4.f), false);

	HomingTarget = Target;
	if (!IsValid(Target) || !ProjectileMovement)
	{
		return;
	}

	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ProjectileMovement->bSweepCollision = false;
	ProjectileMovement->bIsHomingProjectile = true;
	ProjectileMovement->HomingTargetComponent = Target->GetRootComponent();
	ProjectileMovement->HomingAccelerationMagnitude = FMath::Max(Speed * 15.f, 25000.f);
	SetActorTickEnabled(true);
}

void AMobaProjectile::BeginPlay()
{
	Super::BeginPlay();
	ApplyLook();
	if (bHideVisuals)
	{
		HideAllVisuals();
	}
}

void AMobaProjectile::SetImpactRadius(float Radius)
{
	ImpactRadius = FMath::Max(0.f, Radius);
}

void AMobaProjectile::SetExplodeAtZ(float Z)
{
	ExplodeAtZ = Z;
	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(true);
	if (ProjectileMovement && Collision)
	{
		ProjectileMovement->SetUpdatedComponent(Collision);
		ProjectileMovement->bSweepCollision = true;
		const float DownSpeed = FMath::Max(ProjectileMovement->MaxSpeed, 1.f);
		ProjectileMovement->Velocity = FVector(0.f, 0.f, -DownSpeed);
		ProjectileMovement->Activate(true);
	}
}

void AMobaProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bConsumed)
	{
		return;
	}

	if (ExplodeAtZ > -1.e8f)
	{
		if (GetActorLocation().Z <= ExplodeAtZ)
		{
			ConsumeAndDestroy(nullptr);
		}
		return;
	}

	if (!ProjectileMovement || !ProjectileMovement->bIsHomingProjectile)
	{
		return;
	}

	AActor* Target = HomingTarget.Get();
	if (!IsValid(Target))
	{
		HomingTarget.Reset();
		ConsumeAndDestroy(nullptr);
		return;
	}

	if (ProjectileMovement && ProjectileMovement->HomingTargetComponent.Get() != Target->GetRootComponent())
	{
		ProjectileMovement->HomingTargetComponent = Target->GetRootComponent();
	}

	if (FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(HomingHitRadius))
	{
		ConsumeAndDestroy(Target);
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
	if (bConsumed || !IsValid(OtherActor) || OtherActor == GetInstigator() || OtherActor == GetOwner() || Cast<AMobaProjectile>(OtherActor))
	{
		return;
	}
	if (HomingTarget.Get() && OtherActor != HomingTarget.Get())
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
	if (bConsumed || !IsValid(OtherActor) || OtherActor == GetInstigator() || OtherActor == GetOwner() || Cast<AMobaProjectile>(OtherActor))
	{
		return;
	}
	if (HomingTarget.Get() && OtherActor != HomingTarget.Get())
	{
		return;
	}
	ConsumeAndDestroy(OtherActor);
}

void AMobaProjectile::OnStop(const FHitResult& ImpactResult)
{
	if (bConsumed)
	{
		return;
	}
	AActor* HitActor = ImpactResult.GetActor();
	if (HitActor == GetInstigator() || HitActor == GetOwner() || Cast<AMobaProjectile>(HitActor))
	{
		return;
	}
	if (HomingTarget.Get() && HitActor != HomingTarget.Get())
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
	SetActorTickEnabled(false);
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->bIsHomingProjectile = false;
		ProjectileMovement->HomingTargetComponent = nullptr;
		ProjectileMovement->bSweepCollision = false;
	}
	if (Collision)
	{
		Collision->SetGenerateOverlapEvents(false);
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	AActor* Source = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();
	TSet<AActor*> Damaged;
	auto TryHit = [this, Source, &Damaged](AActor* Target)
	{
		if (!IsValid(Target) || Damaged.Contains(Target) || Target == GetInstigator() || Target == GetOwner())
		{
			return;
		}
		if (!bCanDamageTowers && Cast<AMobaTower>(Target))
		{
			return;
		}
		if (AMobaBaseCharacter::ApplyMobaDamage(Target, Damage, Source))
		{
			Damaged.Add(Target);
			AMobaBaseCharacter::ApplyMobaEffects(Target, Source, HitEffects, EMobaEffectTarget::HitActor);
			if (Damaged.Num() == 1)
			{
				AMobaBaseCharacter::ApplyMobaEffects(Target, Source, HitEffects, EMobaEffectTarget::Self);
			}
		}
	};

	if (IsValid(DamageTarget) && !bCosmetic && HasAuthority())
	{
		TryHit(DamageTarget);
	}

	if (!bCosmetic && HasAuthority() && ImpactRadius > 0.f)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			TArray<FOverlapResult> Overlaps;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(MobaProjectileSplash), false, this);
			Params.AddIgnoredActor(this);
			if (AActor* Ignore = GetInstigator())
			{
				Params.AddIgnoredActor(Ignore);
			}
			FCollisionObjectQueryParams Objects;
			Objects.AddObjectTypesToQuery(ECC_Pawn);
			Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
			World->OverlapMultiByObjectType(
				Overlaps,
				GetActorLocation(),
				FQuat::Identity,
				Objects,
				FCollisionShape::MakeSphere(ImpactRadius),
				Params);
			for (const FOverlapResult& Overlap : Overlaps)
			{
				TryHit(Overlap.GetActor());
			}
		}
	}

	Destroy();
}

void AMobaProjectile::SpawnDestroyVfx()
{
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown || bHideVisuals || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UMobaSfx::Play(this, DestroySound, DefaultDestroySfx, GetActorLocation());

	if (!DestroyVfxClass)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Instigator = GetInstigator();
	AActor* Vfx = World->SpawnActor<AActor>(
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

void AMobaProjectile::LifeSpanExpired()
{
	if (!bConsumed)
	{
		ConsumeAndDestroy(nullptr);
		return;
	}
	Super::LifeSpanExpired();
}

void AMobaProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		SpawnDestroyVfx();
	}
	Super::EndPlay(EndPlayReason);
}
