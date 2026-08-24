#include "MobaBaseCharacter.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/DamageType.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AMobaPlayerState.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "MobaGameplayAbility.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "MobaAttributeSet.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"

#include "Engine/Texture2D.h"
#include "MobaAbilityHUD.h"
#include "MobaDamageNumber.h"
#include "MobaGameInstance.h"
#include "MobaGoldHUD.h"
#include "MobaRespawnHUD.h"
#include "MobaCrosshairHUD.h"
#include "GameFramework/GameStateBase.h"
#include "DrawDebugHelpers.h"
#include "GA_MobaGroundTarget.h"
#include "MobaGroundMarker.h"
#include "MobaHealthWidget.h"
#include "MobaMinion.h"
#include "MobaNetLibrary.h"
#include "MobaProjectile.h"
#include "MobaShopHUD.h"
#include "MobaInventoryHUD.h"
#include "MobaShop.h"
#include "EngineUtils.h"
#include "MobaTower.h"
#include "Net/UnrealNetwork.h"
#include "InputCoreTypes.h"

AMobaBaseCharacter::AMobaBaseCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		Skel->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;

	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bEnablePhysicsInteraction = false;
	GetCharacterMovement()->MaxDepenetrationWithPawn = 200.f;
	GetCharacterMovement()->MaxDepenetrationWithPawnAsProxy = 100.f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	DefaultMaxWalkSpeed = 500.f;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bDoCollisionTest = false;
	SpringArm->ProbeChannel = ECC_Camera;
	SpringArm->ProbeSize = 8.f;
	SpringArm->bEnableCameraLag = false;
	SpringArm->bEnableCameraRotationLag = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->bUsePawnControlRotation = false;

	Crosshair = CreateDefaultSubobject<UWidgetComponent>(TEXT("Crosshair"));
	Crosshair->SetupAttachment(Camera);
	Crosshair->SetRelativeLocation(FVector(120.f, 0.f, 0.f));
	Crosshair->SetWidgetSpace(EWidgetSpace::Screen);
	Crosshair->SetPivot(FVector2D(0.5f, 0.5f));
	Crosshair->SetDrawSize(FVector2D(32.f, 32.f));
	Crosshair->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Crosshair->SetTickWhenOffscreen(true);
	Crosshair->SetTwoSided(true);
	Crosshair->SetWidgetClass(UMobaCrosshairHUD::StaticClass());
	Crosshair->SetHiddenInGame(true);

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UMobaAttributeSet>(TEXT("AttributeSet"));

	HealthWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthWidget"));
	HealthWidget->SetupAttachment(GetCapsuleComponent());
	HealthWidget->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
	HealthWidget->SetWidgetSpace(EWidgetSpace::Screen);
	HealthWidget->SetPivot(FVector2D(0.5f, 1.f));
	HealthWidget->SetDrawSize(FVector2D(140.f, 40.f));
	HealthWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthWidget->SetWidgetClass(UMobaHealthWidget::StaticClass());

	auto AddOffer = [this](const TCHAR* Name, EMobaShopStat Stat, float Magnitude, float Cost)
	{
		FMobaShopOffer Offer;
		Offer.Name = Name;
		Offer.Stat = Stat;
		Offer.Magnitude = Magnitude;
		Offer.Cost = Cost;
		ShopOffers.Add(Offer);
	};
	AddOffer(TEXT("Damage"), EMobaShopStat::Damage, 0.1f, 100.f);
	AddOffer(TEXT("Energy"), EMobaShopStat::Energy, 20.f, 80.f);
	AddOffer(TEXT("CDR"), EMobaShopStat::CooldownReduction, 0.05f, 120.f);
	AddOffer(TEXT("Health"), EMobaShopStat::Health, 25.f, 80.f);
	AddOffer(TEXT("Resist"), EMobaShopStat::DamageResistance, 0.05f, 120.f);
	AddOffer(TEXT("Move Speed"), EMobaShopStat::MoveSpeed, 25.f, 80.f);
	AddOffer(TEXT("Gold Regen"), EMobaShopStat::GoldRegen, 1.f, 100.f);
}

void AMobaBaseCharacter::PostLoad()
{
	Super::PostLoad();
	EnsureAbilitySlots();
}

void AMobaBaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	EnsureAbilitySlots();

	if (UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		Cap->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Cap->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Cap->SetGenerateOverlapEvents(true);
	}
	TArray<UPrimitiveComponent*> Prims;
	GetComponents<UPrimitiveComponent>(Prims);
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (Prim)
		{
			Prim->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
	}
	if (SpringArm)
	{
		SpringArm->bDoCollisionTest = false;
		SpringArm->bEnableCameraLag = false;
		SpringArm->bEnableCameraRotationLag = false;
	}

	InitAttributeSet();
	if (HealthWidget)
	{
		if (UMobaHealthWidget* UI = Cast<UMobaHealthWidget>(HealthWidget->GetWidget()))
		{
			UI->SetOwnerCharacter(this);
		}
	}

	if (Crosshair)
	{
		Crosshair->InitWidget();
	}
	CreateAbilityHUD();
	RefreshCrosshairVisibility();
	if (IsLocallyControlled())
	{
		RefreshMenuInput();
	}
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(RegenTimer, this, &AMobaBaseCharacter::TickRegen, 0.25f, true);
		ScheduleShopRangeRefresh();
	}
	if (UWorld* World = GetWorld())
	{
		AMobaGroundMarker::DestroyAllFor(World, this);
	}
}

void AMobaBaseCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SanitizeTimedState();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (IsLocallyControlled())
	{
		if (RespawnHUD)
		{
			RespawnHUD->Refresh();
		}

		// Never draw aim + blast together (F19/F21). Leftover aim sits behind the impact.
		if (bGroundAiming && GroundBlastStartTime <= 0.f)
		{
			const FVector Loc = UGA_MobaGroundTarget::TraceGroundAim(this, GroundAimMaxRange);
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
			const float Radius = GroundBlastRadius * FMath::Clamp(Elapsed / FMath::Max(GroundBlastDuration, 0.05f), 0.f, 1.f);
			DrawDebugSphere(World, GroundBlastLoc, Radius, 24, FColor::Orange, false, -1.f, 0, 4.f);
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
			const float HalfH = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 96.f;
			const FVector Center = GetActorLocation() - FVector(0.f, 0.f, HalfH - 6.f);
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
}

void AMobaBaseCharacter::StartGroundAimDebug(float Radius, float MaxRange)
{
	bGroundAiming = true;
	GroundAimRadius = Radius;
	GroundAimMaxRange = MaxRange;
}

void AMobaBaseCharacter::StopGroundAimDebug()
{
	bGroundAiming = false;
}

void AMobaBaseCharacter::PlayGroundBlastDebug(FVector Location, float Radius, float Lifetime)
{
	StopGroundAimDebug();
	if (GroundBlastStartTime > 0.f
		&& GetWorld()
		&& (GetWorld()->GetTimeSeconds() - GroundBlastStartTime) < 0.2f
		&& FVector::DistSquared(GroundBlastLoc, Location) > 1.f)
	{
		// Same-frame second spawn (predicted + authority) is the extra mesh behind the first.
		return;
	}
	GroundBlastLoc = Location;
	GroundBlastRadius = Radius;
	GroundBlastDuration = FMath::Max(Lifetime, 0.05f);
	GroundBlastStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void AMobaBaseCharacter::PlayFireRingDebug(float Radius, float Lifetime)
{
	FireRingRadius = FMath::Max(Radius, 20.f);
	FireRingDuration = FMath::Max(Lifetime, 0.05f);
	FireRingStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void AMobaBaseCharacter::InitAttributeSet()
{
	if (!AttributeSet)
	{
		return;
	}

	AttributeSet->InitMaxHealth(DefaultMaxHealth);
	AttributeSet->InitHealth(DefaultMaxHealth);
	AttributeSet->InitMaxEnergy(DefaultMaxEnergy);
	AttributeSet->InitEnergy(DefaultMaxEnergy);
	AttributeSet->InitHealthRegen(DefaultHealthRegen);
	AttributeSet->InitEnergyRegen(DefaultEnergyRegen);
	AttributeSet->InitGold(StartingGold);
	AttributeSet->InitGoldOnKill(DefaultGoldOnKill);
	AttributeSet->InitGoldRegen(DefaultGoldRegen);
	AttributeSet->InitDamageModifier(DefaultDamageModifier);
	AttributeSet->InitCooldownReduction(DefaultCooldownReduction);
	AttributeSet->InitDamageResistance(DefaultDamageResistance);
	AttributeSet->InitMoveSpeed(DefaultMoveSpeed);
	DefaultMaxWalkSpeed = DefaultMoveSpeed;
	RefreshMoveSpeed();
}

void AMobaBaseCharacter::SetTeamId(int32 InTeam)
{
	TeamID = InTeam;
	ScheduleShopRangeRefresh();
}

float AMobaBaseCharacter::GetServerTimeSeconds() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}
		return World->GetTimeSeconds();
	}
	return 0.f;
}

float AMobaBaseCharacter::GetRespawnRemaining() const
{
	if (!bDead)
	{
		return 0.f;
	}
	if (RespawnAtTime > 0.f)
	{
		return FMath::Max(0.f, RespawnAtTime - GetServerTimeSeconds());
	}
	return RespawnDelay;
}

void AMobaBaseCharacter::SyncTeamFromPlayerState()
{
	if (const AMobaPlayerState* PS = GetPlayerState<AMobaPlayerState>())
	{
		if (PS->TeamID != 0)
		{
			TeamID = PS->TeamID;
		}
	}
}

void AMobaBaseCharacter::SnapFacingToSpawn(const FRotator& SpawnRot)
{
	const FRotator Yaw(0.f, SpawnRot.Yaw, 0.f);
	SetActorRotation(Yaw);
	AController* C = GetController();
	if (!C)
	{
		return;
	}
	C->SetControlRotation(Yaw);
	if (APlayerController* PC = Cast<APlayerController>(C))
	{
		PC->ClientSetRotation(Yaw, true);
	}
}

void AMobaBaseCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	SnapFacingToSpawn(GetActorRotation());
	SyncTeamFromPlayerState();
	ScheduleShopRangeRefresh();

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	EnsureAbilitySlots();
	for (int32 i = 0; i < AbilitySlots.Num(); ++i)
	{
		GrantAbility(AbilitySlots[i].Ability, i);
	}
	CreateAbilityHUD();
	RefreshCrosshairVisibility();
	if (IsLocallyControlled())
	{
		RefreshMenuInput();
	}
}

void AMobaBaseCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	SyncTeamFromPlayerState();

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	CreateAbilityHUD();
}

UAbilitySystemComponent* AMobaBaseCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

float AMobaBaseCharacter::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.f;
}

float AMobaBaseCharacter::GetMaxHealth() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : 0.f;
}

float AMobaBaseCharacter::GetEnergy() const
{
	return AttributeSet ? AttributeSet->GetEnergy() : 0.f;
}

float AMobaBaseCharacter::GetMaxEnergy() const
{
	return AttributeSet ? AttributeSet->GetMaxEnergy() : 0.f;
}

float AMobaBaseCharacter::GetGold() const
{
	return AttributeSet ? AttributeSet->GetGold() : 0.f;
}

float AMobaBaseCharacter::GetGoldOnKill() const
{
	return AttributeSet ? AttributeSet->GetGoldOnKill() : 0.f;
}

void AMobaBaseCharacter::AddGold(float Amount)
{
	if (!HasAuthority() || !AttributeSet || Amount <= 0.f)
	{
		return;
	}
	AttributeSet->SetGold(AttributeSet->GetGold() + Amount);
}

void AMobaBaseCharacter::NotifyDealtDamage(FVector Location, float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}
	if (IsLocallyControlled())
	{
		ClientShowDamageNumber_Implementation(Location, Amount);
		return;
	}
	ClientShowDamageNumber(Location, Amount);
}

void AMobaBaseCharacter::NotifyTakenDamage(FVector Location, float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}
	if (IsLocallyControlled())
	{
		ClientShowDamageNumber_Implementation(Location, Amount);
		return;
	}
	ClientShowDamageNumber(Location, Amount);
}

void AMobaBaseCharacter::NotifyGainedGold(FVector Location, float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}
	if (IsLocallyControlled())
	{
		ClientShowGoldNumber_Implementation(Location, Amount);
		return;
	}
	ClientShowGoldNumber(Location, Amount);
}

void AMobaBaseCharacter::ClientShowDamageNumber_Implementation(FVector Location, float Amount)
{
	SpawnFloatingNumber(Location, Amount, false);
}

void AMobaBaseCharacter::ClientShowGoldNumber_Implementation(FVector Location, float Amount)
{
	SpawnFloatingNumber(Location, Amount, true);
}

void AMobaBaseCharacter::SpawnFloatingNumber(FVector Location, float Amount, bool bGold)
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
	Params.Owner = this;
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

void AMobaBaseCharacter::SpendGold(float Amount)
{
	if (!HasAuthority() || !AttributeSet || Amount <= 0.f)
	{
		return;
	}
	AttributeSet->SetGold(FMath::Max(0.f, AttributeSet->GetGold() - Amount));
}

void AMobaBaseCharacter::NotifyEnteredShop()
{
	RefreshShopRange();
}

void AMobaBaseCharacter::NotifyLeftShop()
{
	RefreshShopRange();
}

void AMobaBaseCharacter::RefreshShopRange()
{
	if (!HasAuthority() || bIgnoreShopRangeChanges)
	{
		return;
	}

	SyncTeamFromPlayerState();

	int32 Count = 0;
	const int32 HeroTeam = GetTeamId();
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AMobaShop> It(World); It; ++It)
		{
			AMobaShop* Shop = *It;
			if (!Shop)
			{
				continue;
			}
			const int32 ShopTeam = Shop->GetTeamId();
			if (ShopTeam != 0 && HeroTeam != 0 && ShopTeam != HeroTeam)
			{
				continue;
			}
			if (Shop->ContainsPawn(this))
			{
				++Count;
			}
		}
	}

	ShopRangeCount = Count;
	const bool bNowIn = Count > 0;
	if (bInShopRange == bNowIn)
	{
		return;
	}
	bInShopRange = bNowIn;
	OnRep_InShopRange();
}

void AMobaBaseCharacter::ScheduleShopRangeRefresh()
{
	RefreshShopRange();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &AMobaBaseCharacter::RefreshShopRange));
	}
}

void AMobaBaseCharacter::OnRep_InShopRange()
{
	if (!CanUseShop() && bShopOpen)
	{
		SetShopOpen(false);
	}
}

bool AMobaBaseCharacter::CanUseShop() const
{
	return bDead || bInShopRange;
}

bool AMobaBaseCharacter::CanBuyAnything() const
{
	if (!CanUseShop())
	{
		return false;
	}
	for (int32 i = 0; i < ShopOffers.Num(); ++i)
	{
		if (CanBuyShopOffer(i))
		{
			return true;
		}
	}
	return false;
}

bool AMobaBaseCharacter::CanApplyShopOffer(const FMobaShopOffer& Offer) const
{
	if (!AttributeSet || Offer.Magnitude <= 0.f)
	{
		return false;
	}
	switch (Offer.Stat)
	{
	case EMobaShopStat::CooldownReduction:
		return AttributeSet->GetCooldownReduction() < 0.8f - KINDA_SMALL_NUMBER;
	case EMobaShopStat::DamageResistance:
		return AttributeSet->GetDamageResistance() < 0.9f - KINDA_SMALL_NUMBER;
	default:
		return true;
	}
}

bool AMobaBaseCharacter::CanBuyShopOffer(int32 Index) const
{
	if (!CanUseShop() || !ShopOffers.IsValidIndex(Index))
	{
		return false;
	}
	const FMobaShopOffer& Offer = ShopOffers[Index];
	return GetGold() + 0.01f >= Offer.Cost && CanApplyShopOffer(Offer);
}

void AMobaBaseCharacter::TryBuyShopOffer(int32 Index)
{
	if (!CanBuyShopOffer(Index))
	{
		return;
	}
	ServerBuyShopOffer(Index);
}

void AMobaBaseCharacter::ServerBuyShopOffer_Implementation(int32 Index)
{
	if (!CanBuyShopOffer(Index) || !AttributeSet)
	{
		return;
	}

	const FMobaShopOffer Offer = ShopOffers[Index];
	SpendGold(Offer.Cost);
	ApplyShopOffer(Offer);
	PurchasedOffers.Add(Offer);
}

void AMobaBaseCharacter::ServerRequestPlayAgain_Implementation()
{
	if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
	{
		GI->RestartMatch();
	}
}

void AMobaBaseCharacter::ServerRequestReturnToMenu_Implementation()
{
	if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
	{
		GI->ReturnToMenu();
	}
}

void AMobaBaseCharacter::ApplyShopOffer(const FMobaShopOffer& Offer)
{
	if (!AttributeSet)
	{
		return;
	}

	switch (Offer.Stat)
	{
	case EMobaShopStat::Damage:
		AttributeSet->SetDamageModifier(AttributeSet->GetDamageModifier() + Offer.Magnitude);
		break;
	case EMobaShopStat::Energy:
		AttributeSet->SetMaxEnergy(AttributeSet->GetMaxEnergy() + Offer.Magnitude);
		AttributeSet->SetEnergy(AttributeSet->GetEnergy() + Offer.Magnitude);
		break;
	case EMobaShopStat::CooldownReduction:
		AttributeSet->SetCooldownReduction(FMath::Clamp(
			AttributeSet->GetCooldownReduction() + Offer.Magnitude, 0.f, 0.8f));
		break;
	case EMobaShopStat::Health:
		AttributeSet->SetMaxHealth(AttributeSet->GetMaxHealth() + Offer.Magnitude);
		AttributeSet->SetHealth(AttributeSet->GetHealth() + Offer.Magnitude);
		break;
	case EMobaShopStat::DamageResistance:
		AttributeSet->SetDamageResistance(FMath::Clamp(
			AttributeSet->GetDamageResistance() + Offer.Magnitude, 0.f, 0.9f));
		break;
	case EMobaShopStat::MoveSpeed:
		AttributeSet->SetMoveSpeed(AttributeSet->GetMoveSpeed() + Offer.Magnitude);
		RefreshMoveSpeed();
		break;
	case EMobaShopStat::GoldRegen:
		AttributeSet->SetGoldRegen(AttributeSet->GetGoldRegen() + Offer.Magnitude);
		break;
	default:
		break;
	}
}

bool AMobaBaseCharacter::HasEnergy(float Cost) const
{
	return Cost <= 0.f || GetEnergy() + 0.01f >= Cost;
}

void AMobaBaseCharacter::SpendEnergy(float Cost)
{
	if (!HasAuthority() || !AttributeSet || Cost <= 0.f)
	{
		return;
	}
	AttributeSet->SetEnergy(FMath::Max(0.f, AttributeSet->GetEnergy() - Cost));
}

void AMobaBaseCharacter::TickRegen()
{
	if (!HasAuthority() || !AttributeSet)
	{
		return;
	}

	if (!bDead)
	{
		RefreshShopRange();
	}

	const float Dt = 0.25f;
	if (AttributeSet->GetGoldRegen() > 0.f)
	{
		AttributeSet->SetGold(AttributeSet->GetGold() + AttributeSet->GetGoldRegen() * Dt);
	}

	if (bDead)
	{
		return;
	}

	float HealthRate = AttributeSet->GetHealthRegen();
	float EnergyRate = AttributeSet->GetEnergyRegen();
	if (bInShopRange)
	{
		HealthRate += ShopHealthRegenBonus;
		EnergyRate += ShopEnergyRegenBonus;
	}
	AttributeSet->SetHealth(FMath::Min(AttributeSet->GetMaxHealth(), AttributeSet->GetHealth() + HealthRate * Dt));
	AttributeSet->SetEnergy(FMath::Min(AttributeSet->GetMaxEnergy(), AttributeSet->GetEnergy() + EnergyRate * Dt));
}

bool AMobaBaseCharacter::ApplyMobaDamage(AActor* Target, float Amount, AActor* Instigator)
{
	if (!IsValid(Target) || Amount <= 0.f || !Target->HasAuthority() || Target == Instigator)
	{
		return false;
	}

	UMobaAttributeSet* Set = const_cast<UMobaAttributeSet*>(UMobaAttributeSet::GetFromActor(Target));
	if (!Set || Set->GetHealth() <= 0.f)
	{
		return false;
	}

	if (!MobaIsEnemy(Instigator, Target))
	{
		return false;
	}

	float Incoming = Amount;
	const AMobaTower* AttackingTower = Cast<AMobaTower>(Instigator);
	if (AttackingTower && Cast<AMobaMinion>(Target))
	{
		Incoming = Set->GetMaxHealth() * FMath::Clamp(AttackingTower->GetMinionMaxHealthDamage(), 0.f, 1.f);
	}
	else
	{
		if (const UMobaAttributeSet* AttackerSet = UMobaAttributeSet::GetFromActor(Instigator))
		{
			Incoming *= FMath::Max(0.f, AttackerSet->GetDamageModifier());
		}
		Incoming *= 1.f - FMath::Clamp(Set->GetDamageResistance(), 0.f, 0.9f);
	}
	Incoming = FMath::Max(0.f, Incoming);

	Set->SetHealth(FMath::Max(0.f, Set->GetHealth() - Incoming));
	if (Incoming > 0.f)
	{
		const FVector NumberLoc = Target->GetActorLocation() + FVector(0.f, 0.f, 90.f);
		if (AMobaBaseCharacter* Dealer = Cast<AMobaBaseCharacter>(Instigator))
		{
			if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Target))
			{
				Hero->NotePlayerDamageFrom(Dealer);
			}
			else if (AMobaMinion* Minion = Cast<AMobaMinion>(Target))
			{
				Minion->NotePlayerDamageFrom(Dealer);
			}
			Dealer->NotifyDealtDamage(NumberLoc, Incoming);
		}
		if (AMobaBaseCharacter* HurtHero = Cast<AMobaBaseCharacter>(Target))
		{
			HurtHero->NotifyTakenDamage(NumberLoc, Incoming);
		}
		if (AMobaMinion* Minion = Cast<AMobaMinion>(Target))
		{
			Minion->NotifyDamagedBy(Instigator);
		}
	}
	if (Set->GetHealth() <= 0.f)
	{
		if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Target))
		{
			Hero->HandleDeath(Instigator);
		}
		else if (AMobaMinion* DeadMinion = Cast<AMobaMinion>(Target))
		{
			DeadMinion->HandleDeath(Instigator);
		}
		else if (AMobaTower* Tower = Cast<AMobaTower>(Target))
		{
			Tower->HandleDeath();
		}
	}
	return true;
}

void AMobaBaseCharacter::AwardKillGold(AActor* Victim, AActor* Killer)
{
	if (!IsValid(Victim))
	{
		return;
	}
	if (!Cast<AMobaBaseCharacter>(Victim) && !Cast<AMobaMinion>(Victim))
	{
		return;
	}

	const UMobaAttributeSet* VictimSet = UMobaAttributeSet::GetFromActor(Victim);
	if (!VictimSet)
	{
		return;
	}
	const float Gold = VictimSet->GetGoldOnKill();
	if (Gold <= 0.f)
	{
		return;
	}

	AMobaBaseCharacter* Recipient = Cast<AMobaBaseCharacter>(Killer);
	if (!Recipient)
	{
		if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Victim))
		{
			Recipient = Hero->GetPlayerKillCredit();
		}
		else if (AMobaMinion* Minion = Cast<AMobaMinion>(Victim))
		{
			Recipient = Minion->GetPlayerKillCredit();
		}
	}
	if (Recipient)
	{
		Recipient->AddGold(Gold);
		Recipient->NotifyGainedGold(Victim->GetActorLocation() + FVector(0.f, 0.f, 90.f), Gold);
	}
}

void AMobaBaseCharacter::NotePlayerDamageFrom(AMobaBaseCharacter* Player)
{
	if (!HasAuthority() || !IsValid(Player) || Player == this)
	{
		return;
	}

	const float Window = FMath::Max(0.f, PlayerKillCreditSeconds);
	if (Window <= KINDA_SMALL_NUMBER)
	{
		ClearPlayerKillCredit();
		return;
	}

	LastPlayerDamager = Player;
	PlayerKillCreditUntilTime = GetServerTimeSeconds() + Window;
	GetWorldTimerManager().ClearTimer(PlayerKillCreditTimer);
	GetWorldTimerManager().SetTimer(
		PlayerKillCreditTimer,
		this,
		&AMobaBaseCharacter::ClearPlayerKillCredit,
		Window,
		false);
}

AMobaBaseCharacter* AMobaBaseCharacter::GetPlayerKillCredit() const
{
	if (IsTimerExpired(PlayerKillCreditUntilTime))
	{
		return nullptr;
	}
	AMobaBaseCharacter* Player = LastPlayerDamager.Get();
	return IsValid(Player) ? Player : nullptr;
}

void AMobaBaseCharacter::ClearPlayerKillCredit()
{
	LastPlayerDamager.Reset();
	PlayerKillCreditUntilTime = 0.f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlayerKillCreditTimer);
	}
}

void AMobaBaseCharacter::ApplyMobaEffects(
	AActor* HitActor,
	AActor* Instigator,
	const TArray<FMobaEffectSpec>& Effects,
	EMobaEffectTarget Filter)
{
	for (const FMobaEffectSpec& Spec : Effects)
	{
		if (Spec.Target != Filter)
		{
			continue;
		}

		AActor* Target = (Spec.Target == EMobaEffectTarget::Self) ? Instigator : HitActor;
		if (!IsValid(Target) || !Target->HasAuthority())
		{
			continue;
		}

		if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Target))
		{
			Hero->ApplyStatus(Spec);
			continue;
		}
		if (AMobaMinion* Minion = Cast<AMobaMinion>(Target))
		{
			Minion->ApplyStatus(Spec);
			continue;
		}
		if (Spec.Type == EMobaEffectType::Heal)
		{
			const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Target);
			UAbilitySystemComponent* ASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr;
			UMobaAttributeSet* Set = ASC ? const_cast<UMobaAttributeSet*>(ASC->GetSet<UMobaAttributeSet>()) : nullptr;
			if (Set)
			{
				Set->SetHealth(FMath::Min(Set->GetMaxHealth(), Set->GetHealth() + Spec.Magnitude));
			}
		}
	}
}

void AMobaBaseCharacter::ApplyStatus(const FMobaEffectSpec& Spec)
{
	if (bDead)
	{
		return;
	}

	switch (Spec.Type)
	{
	case EMobaEffectType::Heal:
		if (AttributeSet && Spec.Magnitude > 0.f)
		{
			AttributeSet->SetHealth(FMath::Min(AttributeSet->GetMaxHealth(), AttributeSet->GetHealth() + Spec.Magnitude));
		}
		break;

	case EMobaEffectType::Stun:
		if (Spec.Duration > 0.f)
		{
			bStunned = true;
			StunUntilTime = GetServerTimeSeconds() + Spec.Duration;
			SetStatusTag("State.Stunned", true);
			GetWorldTimerManager().SetTimer(StunTimer, this, &AMobaBaseCharacter::ClearStun, Spec.Duration, false);
			CancelHoldAbility();
			RefreshMoveSpeed();
		}
		break;

	case EMobaEffectType::Slow:
		if (Spec.Duration > 0.f)
		{
			SlowMul = FMath::Clamp(Spec.Magnitude, 0.f, 1.f);
			SlowUntilTime = GetServerTimeSeconds() + Spec.Duration;
			SetStatusTag("State.Slowed", true);
			GetWorldTimerManager().SetTimer(SlowTimer, this, &AMobaBaseCharacter::ClearSlow, Spec.Duration, false);
			RefreshMoveSpeed();
		}
		break;

	case EMobaEffectType::MoveSpeed:
		if (Spec.Duration > 0.f)
		{
			HasteMul = FMath::Max(Spec.Magnitude, 0.f);
			HasteUntilTime = GetServerTimeSeconds() + Spec.Duration;
			SetStatusTag("State.Hasted", true);
			GetWorldTimerManager().SetTimer(HasteTimer, this, &AMobaBaseCharacter::ClearHaste, Spec.Duration, false);
			RefreshMoveSpeed();
		}
		break;

	default:
		break;
	}
}

void AMobaBaseCharacter::SetStatusTag(FName TagName, bool bEnabled)
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName, false);
	if (!Tag.IsValid())
	{
		return;
	}

	if (bEnabled)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(Tag, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
	else
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(Tag, 0, EGameplayTagReplicationState::TagAndCountToAll);
	}
}

void AMobaBaseCharacter::RefreshMoveSpeed()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement || bDead)
	{
		return;
	}

	if (bStunned)
	{
		Movement->DisableMovement();
		Movement->StopMovementImmediately();
		return;
	}

	if (Movement->MovementMode == MOVE_None)
	{
		Movement->SetMovementMode(MOVE_Walking);
	}

	float Mul = SlowMul * HasteMul;
	const bool bCasting = !IsTimerExpired(PlantedUntilTime)
		&& (!IsLocallyControlled() || PlantedAbilityCount > 0);
	if (bCasting)
	{
		Mul *= 0.5f;
	}
	const float BaseSpeed = (AttributeSet && AttributeSet->GetMoveSpeed() > 0.f)
		? AttributeSet->GetMoveSpeed()
		: DefaultMaxWalkSpeed;
	Movement->MaxWalkSpeed = BaseSpeed * Mul;
}

void AMobaBaseCharacter::ClearStun()
{
	bStunned = false;
	StunUntilTime = 0.f;
	GetWorldTimerManager().ClearTimer(StunTimer);
	SetStatusTag("State.Stunned", false);
	RefreshMoveSpeed();
}

void AMobaBaseCharacter::ClearSlow()
{
	SlowMul = 1.f;
	SlowUntilTime = 0.f;
	GetWorldTimerManager().ClearTimer(SlowTimer);
	SetStatusTag("State.Slowed", false);
	RefreshMoveSpeed();
}

void AMobaBaseCharacter::ClearHaste()
{
	HasteMul = 1.f;
	HasteUntilTime = 0.f;
	GetWorldTimerManager().ClearTimer(HasteTimer);
	SetStatusTag("State.Hasted", false);
	RefreshMoveSpeed();
}

void AMobaBaseCharacter::ClearAllStatus()
{
	GetWorldTimerManager().ClearTimer(StunTimer);
	GetWorldTimerManager().ClearTimer(SlowTimer);
	GetWorldTimerManager().ClearTimer(HasteTimer);
	GetWorldTimerManager().ClearTimer(PlantedTimer);
	bStunned = false;
	SlowMul = 1.f;
	HasteMul = 1.f;
	PlantedAbilityCount = 0;
	bPlanted = false;
	PlantedUntilTime = 0.f;
	StunUntilTime = 0.f;
	SlowUntilTime = 0.f;
	HasteUntilTime = 0.f;
	SetStatusTag("State.Stunned", false);
	SetStatusTag("State.Slowed", false);
	SetStatusTag("State.Hasted", false);
	RefreshMoveSpeed();
}

bool AMobaBaseCharacter::IsTimerExpired(float UntilTime) const
{
	return UntilTime <= KINDA_SMALL_NUMBER || GetServerTimeSeconds() >= UntilTime;
}

void AMobaBaseCharacter::SanitizeTimedState()
{
	if (bStunned && IsTimerExpired(StunUntilTime))
	{
		ClearStun();
	}
	if (SlowMul < 0.99f && IsTimerExpired(SlowUntilTime))
	{
		ClearSlow();
	}
	if (HasteMul > 1.01f && IsTimerExpired(HasteUntilTime))
	{
		ClearHaste();
	}
	if ((bPlanted || PlantedAbilityCount > 0) && IsTimerExpired(PlantedUntilTime))
	{
		EndPlantedFromTimer();
	}
	if (LastPlayerDamager.IsValid() && IsTimerExpired(PlayerKillCreditUntilTime))
	{
		ClearPlayerKillCredit();
	}

	TArray<FGameplayTag> Expired;
	for (const TPair<FGameplayTag, float>& Pair : CooldownEndTimes)
	{
		if (IsTimerExpired(Pair.Value))
		{
			Expired.Add(Pair.Key);
		}
	}
	for (const FMobaTimerSanity& Entry : CooldownSanity)
	{
		if (Entry.Tag.IsValid() && IsTimerExpired(Entry.UntilTime))
		{
			Expired.AddUnique(Entry.Tag);
		}
	}
	for (const FGameplayTag& Tag : Expired)
	{
		ClearCooldown(Tag);
	}
}

void AMobaBaseCharacter::OnRep_MoveStatus()
{
	SanitizeTimedState();
	if (bStunned)
	{
		CancelHoldAbility();
	}
	RefreshMoveSpeed();
}

void AMobaBaseCharacter::StartCooldown(FGameplayTag Tag, float Duration)
{
	if (!Tag.IsValid() || Duration <= 0.f || !AbilitySystemComponent)
	{
		return;
	}

	float Scaled = Duration;
	if (AttributeSet)
	{
		const float Cdr = FMath::Clamp(AttributeSet->GetCooldownReduction(), 0.f, 0.8f);
		Scaled = Duration * (1.f - Cdr);
	}
	Scaled = FMath::Max(Scaled, 0.05f);
	if (HasAuthority() && !IsLocallyControlled())
	{
		Scaled = UMobaNetLibrary::CompensateCooldown(
			Scaled,
			UMobaNetLibrary::GetRoundTripPingSeconds(GetPlayerState()));
	}

	AbilitySystemComponent->SetLooseGameplayTagCount(Tag, 1);
	CooldownDurations.FindOrAdd(Tag) = Scaled;
	const float UntilTime = GetServerTimeSeconds() + Scaled;
	CooldownEndTimes.FindOrAdd(Tag) = UntilTime;
	UpsertCooldownSanity(Tag, UntilTime);
	FTimerHandle& Handle = CooldownHandles.FindOrAdd(Tag);
	GetWorldTimerManager().ClearTimer(Handle);
	GetWorldTimerManager().SetTimer(
		Handle,
		FTimerDelegate::CreateUObject(this, &AMobaBaseCharacter::ClearCooldown, Tag),
		Scaled,
		false);
}

void AMobaBaseCharacter::EnsureAbilitySlots()
{
	if (AbilitySlots.Num() == 0)
	{
		const TSubclassOf<UGameplayAbility> LegacyAbilities[4] = { Ability1, Ability2, Ability3, Ability4 };
		UInputAction* LegacyInputs[4] = { Ability1Input, Ability2Input, Ability3Input, Ability4Input };
		static const TCHAR* LegacyLabels[4] = { TEXT("LMB"), TEXT("Q"), TEXT("SHIFT"), TEXT("E") };
		for (int32 i = 0; i < 4; ++i)
		{
			if (!LegacyAbilities[i] && !LegacyInputs[i])
			{
				continue;
			}
			FMobaAbilityBind Slot;
			Slot.Ability = LegacyAbilities[i];
			Slot.Input = LegacyInputs[i];
			Slot.KeyLabel = LegacyLabels[i];
			AbilitySlots.Add(Slot);
		}
	}

	if (AbilitySlots.Num() == 0)
	{
		for (UClass* Super = GetClass()->GetSuperClass(); Super; Super = Super->GetSuperClass())
		{
			if (!Super->IsChildOf(StaticClass()))
			{
				break;
			}
			const AMobaBaseCharacter* ParentCDO = Super->GetDefaultObject<AMobaBaseCharacter>();
			if (ParentCDO && ParentCDO != this && ParentCDO->AbilitySlots.Num() > 0)
			{
				AbilitySlots = ParentCDO->AbilitySlots;
				break;
			}
		}
	}
}

int32 AMobaBaseCharacter::GetAbilitySlotCount() const
{
	return AbilitySlots.Num();
}

FString AMobaBaseCharacter::GetAbilityKeyLabel(int32 Index) const
{
	if (!AbilitySlots.IsValidIndex(Index))
	{
		return FString();
	}
	if (!AbilitySlots[Index].KeyLabel.IsEmpty())
	{
		return AbilitySlots[Index].KeyLabel;
	}
	return FString::FromInt(Index + 1);
}

TSubclassOf<UGameplayAbility> AMobaBaseCharacter::GetAbilitySlot(int32 Index) const
{
	return AbilitySlots.IsValidIndex(Index) ? AbilitySlots[Index].Ability : nullptr;
}

FGameplayTag AMobaBaseCharacter::AbilitySlotTag(int32 SlotIndex)
{
	if (SlotIndex < 0)
	{
		return FGameplayTag();
	}
	return FGameplayTag::RequestGameplayTag(
		FName(*FString::Printf(TEXT("Ability.%d"), SlotIndex + 1)),
		false);
}

FGameplayTag AMobaBaseCharacter::GetAbilityTagForSlot(int32 SlotIndex) const
{
	return AbilitySlotTag(SlotIndex);
}

void AMobaBaseCharacter::SetPendingAbilityLocation(const FVector& Location)
{
	PendingAbilityLocation = Location;
	bHasPendingAbilityLocation = true;
}

FVector AMobaBaseCharacter::ConsumePendingAbilityLocation()
{
	bHasPendingAbilityLocation = false;
	return PendingAbilityLocation;
}

FGameplayTag AMobaBaseCharacter::GetCooldownTagForAbilityClass(TSubclassOf<UGameplayAbility> AbilityClass) const
{
	UClass* Query = AbilityClass.Get();
	if (!Query)
	{
		return FGameplayTag();
	}
	Query = Query->GetAuthoritativeClass();

	int32 ExactIndex = INDEX_NONE;
	int32 ChildIndex = INDEX_NONE;
	for (int32 i = 0; i < AbilitySlots.Num(); ++i)
	{
		UClass* SlotClass = AbilitySlots[i].Ability.Get();
		if (!SlotClass)
		{
			continue;
		}
		SlotClass = SlotClass->GetAuthoritativeClass();
		if (Query == SlotClass)
		{
			ExactIndex = i;
			break;
		}
		if (ChildIndex == INDEX_NONE && Query->IsChildOf(SlotClass))
		{
			ChildIndex = i;
		}
	}

	const int32 Index = ExactIndex != INDEX_NONE ? ExactIndex : ChildIndex;
	return Index != INDEX_NONE ? GetAbilityTagForSlot(Index) : FGameplayTag();
}

void AMobaBaseCharacter::GetAbilityHudInfo(int32 Index, UTexture2D*& OutIcon, float& OutRemaining, float& OutDuration) const
{
	OutIcon = nullptr;
	OutRemaining = 0.f;
	OutDuration = 0.f;

	const TSubclassOf<UGameplayAbility> AbilityClass = GetAbilitySlot(Index);
	const UMobaGameplayAbility* CDO = AbilityClass ? AbilityClass->GetDefaultObject<UMobaGameplayAbility>() : nullptr;
	if (!CDO)
	{
		return;
	}

	OutIcon = CDO->Icon;
	if (!OutIcon)
	{
		static const TCHAR* IconPaths[4] = {
			TEXT("/Game/Moba/Art/T_Icon_Melee.T_Icon_Melee"),
			TEXT("/Game/Moba/Art/T_Icon_Skillshot.T_Icon_Skillshot"),
			TEXT("/Game/Moba/Art/T_Icon_Dash.T_Icon_Dash"),
			TEXT("/Game/Moba/Art/T_Icon_Ground.T_Icon_Ground")
		};
		if (Index >= 0)
		{
			OutIcon = LoadObject<UTexture2D>(nullptr, IconPaths[Index % 4]);
		}
	}
	OutDuration = CDO->Cooldown;
	if (AttributeSet)
	{
		const float Cdr = FMath::Clamp(AttributeSet->GetCooldownReduction(), 0.f, 0.8f);
		OutDuration = FMath::Max(CDO->Cooldown * (1.f - Cdr), 0.05f);
	}
	const FGameplayTag SlotCooldown = GetAbilityTagForSlot(Index);
	if (const float* Stored = CooldownDurations.Find(SlotCooldown))
	{
		OutDuration = *Stored;
	}

	if (SlotCooldown.IsValid())
	{
		if (const float* EndTime = CooldownEndTimes.Find(SlotCooldown))
		{
			OutRemaining = FMath::Max(0.f, *EndTime - GetServerTimeSeconds());
		}
		else if (const FTimerHandle* Handle = CooldownHandles.Find(SlotCooldown))
		{
			const float Remaining = GetWorldTimerManager().GetTimerRemaining(*Handle);
			OutRemaining = Remaining > 0.f ? Remaining : 0.f;
		}
	}
}

void AMobaBaseCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	CreateAbilityHUD();
	RefreshCrosshairVisibility();
}

void AMobaBaseCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	SnapFacingToSpawn(GetActorRotation());
	CreateAbilityHUD();
	RefreshCrosshairVisibility();
}

void AMobaBaseCharacter::CreateAbilityHUD()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	RefreshCrosshairVisibility();
	CreateGoldHUD();
	CreateShopHUD();
	CreateInventoryHUD();
	CreateRespawnHUD();

	if (AbilityHUD)
	{
		AbilityHUD->PlaceInViewport();
		if (HealthWidget)
		{
			HealthWidget->SetHiddenInGame(true);
		}
		return;
	}

	AbilityHUD = CreateWidget<UMobaAbilityHUD>(PC, UMobaAbilityHUD::StaticClass());
	if (!AbilityHUD)
	{
		return;
	}

	AbilityHUD->SetOwnerCharacter(this);
	AbilityHUD->PlaceInViewport();
	if (HealthWidget)
	{
		HealthWidget->SetHiddenInGame(true);
	}
}

void AMobaBaseCharacter::CreateRespawnHUD()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	if (RespawnHUD)
	{
		RespawnHUD->SetOwnerCharacter(this);
		RespawnHUD->PlaceInViewport();
		RespawnHUD->Refresh();
		return;
	}

	RespawnHUD = CreateWidget<UMobaRespawnHUD>(PC, UMobaRespawnHUD::StaticClass());
	if (!RespawnHUD)
	{
		return;
	}
	RespawnHUD->SetOwnerCharacter(this);
	RespawnHUD->PlaceInViewport();
}

void AMobaBaseCharacter::RefreshCrosshairVisibility()
{
	if (!Crosshair)
	{
		return;
	}
	const bool bShow = IsLocallyControlled() && !bDead;
	Crosshair->SetHiddenInGame(!bShow);
	Crosshair->SetVisibility(bShow);
}

void AMobaBaseCharacter::CreateGoldHUD()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	if (GoldHUD)
	{
		GoldHUD->PlaceInViewport();
		return;
	}

	GoldHUD = CreateWidget<UMobaGoldHUD>(PC, UMobaGoldHUD::StaticClass());
	if (!GoldHUD)
	{
		return;
	}
	GoldHUD->SetOwnerCharacter(this);
	GoldHUD->PlaceInViewport();
}

void AMobaBaseCharacter::CreateShopHUD()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	if (ShopHUD)
	{
		ShopHUD->PlaceInViewport();
		return;
	}

	ShopHUD = CreateWidget<UMobaShopHUD>(PC, UMobaShopHUD::StaticClass());
	if (!ShopHUD)
	{
		return;
	}
	ShopHUD->SetOwnerCharacter(this);
	ShopHUD->PlaceInViewport();
	ShopHUD->SetShopOpen(false);
}

void AMobaBaseCharacter::CreateInventoryHUD()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	if (InventoryHUD)
	{
		InventoryHUD->PlaceInViewport();
		return;
	}

	InventoryHUD = CreateWidget<UMobaInventoryHUD>(PC, UMobaInventoryHUD::StaticClass());
	if (!InventoryHUD)
	{
		return;
	}
	InventoryHUD->SetOwnerCharacter(this);
	InventoryHUD->PlaceInViewport();
	InventoryHUD->SetInventoryOpen(false);
}

void AMobaBaseCharacter::ToggleShop()
{
	if (!IsLocallyControlled())
	{
		return;
	}
	if (bShopOpen)
	{
		SetShopOpen(false);
		return;
	}
	if (!CanUseShop())
	{
		return;
	}
	CreateShopHUD();
	SetShopOpen(true);
}

void AMobaBaseCharacter::ToggleInventory()
{
	if (!IsLocallyControlled())
	{
		return;
	}
	CreateInventoryHUD();
	SetInventoryOpen(!bInventoryOpen);
}

void AMobaBaseCharacter::ToggleSettings()
{
	if (!IsLocallyControlled())
	{
		return;
	}
	if (UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
	{
		GI->ToggleSettings();
	}
}

void AMobaBaseCharacter::SetShopOpen(bool bOpen)
{
	bShopOpen = bOpen && CanUseShop();
	if (ShopHUD)
	{
		ShopHUD->SetShopOpen(bShopOpen);
	}
	RefreshMenuInput();
}

void AMobaBaseCharacter::SetInventoryOpen(bool bOpen)
{
	bInventoryOpen = bOpen;
	if (InventoryHUD)
	{
		InventoryHUD->SetInventoryOpen(bOpen);
	}
	RefreshMenuInput();
}

void AMobaBaseCharacter::RefreshMenuInput()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (const UMobaGameInstance* GI = GetGameInstance<UMobaGameInstance>())
	{
		if (GI->IsShowingLoading())
		{
			return;
		}
	}

	const bool bNeedMouse = bShopOpen;
	PC->bShowMouseCursor = bNeedMouse;
	if (bNeedMouse)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		if (ShopHUD)
		{
			Mode.SetWidgetToFocus(ShopHUD->TakeWidget());
		}
		PC->SetInputMode(Mode);
		PC->SetIgnoreLookInput(true);
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetIgnoreLookInput(false);
	}
}

void AMobaBaseCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetShopOpen(false);
	SetInventoryOpen(false);
	if (AbilityHUD)
	{
		AbilityHUD->RemoveFromParent();
		AbilityHUD = nullptr;
	}
	if (GoldHUD)
	{
		GoldHUD->RemoveFromParent();
		GoldHUD = nullptr;
	}
	if (ShopHUD)
	{
		ShopHUD->RemoveFromParent();
		ShopHUD = nullptr;
	}
	if (InventoryHUD)
	{
		InventoryHUD->RemoveFromParent();
		InventoryHUD = nullptr;
	}
	if (RespawnHUD)
	{
		RespawnHUD->RemoveFromParent();
		RespawnHUD = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AMobaBaseCharacter::ClearCooldown(FGameplayTag Tag)
{
	if (AbilitySystemComponent && Tag.IsValid())
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(Tag, 0);
	}
	if (FTimerHandle* Handle = CooldownHandles.Find(Tag))
	{
		GetWorldTimerManager().ClearTimer(*Handle);
	}
	CooldownHandles.Remove(Tag);
	CooldownEndTimes.Remove(Tag);
	RemoveCooldownSanity(Tag);
}

void AMobaBaseCharacter::UpsertCooldownSanity(FGameplayTag Tag, float UntilTime)
{
	if (!Tag.IsValid())
	{
		return;
	}
	for (FMobaTimerSanity& Entry : CooldownSanity)
	{
		if (Entry.Tag == Tag)
		{
			Entry.UntilTime = UntilTime;
			return;
		}
	}
	FMobaTimerSanity Entry;
	Entry.Tag = Tag;
	Entry.UntilTime = UntilTime;
	CooldownSanity.Add(Entry);
}

void AMobaBaseCharacter::RemoveCooldownSanity(FGameplayTag Tag)
{
	for (int32 i = CooldownSanity.Num() - 1; i >= 0; --i)
	{
		if (CooldownSanity[i].Tag == Tag)
		{
			CooldownSanity.RemoveAt(i);
		}
	}
}

FVector AMobaBaseCharacter::GetMoveDashDirection() const
{
	FVector Input = GetPendingMovementInputVector();
	if (Input.IsNearlyZero())
	{
		Input = GetLastMovementInputVector();
	}

	Input.Z = 0.f;
	if (Input.IsNearlyZero())
	{
		const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
		Input = Yaw.Vector();
		Input.Z = 0.f;
	}

	return Input.GetSafeNormal();
}

void AMobaBaseCharacter::BeginPlantedAbility(float MaxDuration)
{
	PlantedAbilityCount++;
	const float Cap = FMath::Clamp(MaxDuration, 0.2f, 2.5f);
	const float Now = GetServerTimeSeconds();
	PlantedUntilTime = FMath::Max(PlantedUntilTime, Now + Cap);
	bPlanted = true;
	const float Remaining = FMath::Max(0.05f, PlantedUntilTime - Now);
	GetWorldTimerManager().SetTimer(PlantedTimer, this, &AMobaBaseCharacter::EndPlantedFromTimer, Remaining, false);
	RefreshMoveSpeed();
}

void AMobaBaseCharacter::EndPlantedAbility()
{
	if (PlantedAbilityCount > 0)
	{
		PlantedAbilityCount--;
	}
	if (PlantedAbilityCount <= 0)
	{
		EndPlantedFromTimer();
		return;
	}
	RefreshMoveSpeed();
}

void AMobaBaseCharacter::EndPlantedFromTimer()
{
	GetWorldTimerManager().ClearTimer(PlantedTimer);
	PlantedAbilityCount = 0;
	bPlanted = false;
	PlantedUntilTime = 0.f;
	RefreshMoveSpeed();
}

void AMobaBaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMobaBaseCharacter, TeamID);
	DOREPLIFETIME(AMobaBaseCharacter, bDead);
	DOREPLIFETIME(AMobaBaseCharacter, bStunned);
	DOREPLIFETIME(AMobaBaseCharacter, SlowMul);
	DOREPLIFETIME(AMobaBaseCharacter, HasteMul);
	DOREPLIFETIME(AMobaBaseCharacter, bPlanted);
	DOREPLIFETIME(AMobaBaseCharacter, PlantedUntilTime);
	DOREPLIFETIME(AMobaBaseCharacter, StunUntilTime);
	DOREPLIFETIME(AMobaBaseCharacter, SlowUntilTime);
	DOREPLIFETIME(AMobaBaseCharacter, HasteUntilTime);
	DOREPLIFETIME(AMobaBaseCharacter, CooldownSanity);
	DOREPLIFETIME(AMobaBaseCharacter, bInShopRange);
	DOREPLIFETIME(AMobaBaseCharacter, PurchasedOffers);
	DOREPLIFETIME(AMobaBaseCharacter, RespawnAtTime);
}

void AMobaBaseCharacter::HandleDeath(AActor* Killer)
{
	if (bDead || !HasAuthority())
	{
		return;
	}

	AwardKillGold(this, Killer);
	bDead = true;
	ClearPlayerKillCredit();
	ClearAllStatus();
	CancelHoldAbility();
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAbilities();
		AbilitySystemComponent->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Dead"), false));
	}
	RespawnAtTime = GetServerTimeSeconds() + RespawnDelay;
	ApplyDeathPresentation();
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AMobaBaseCharacter::Respawn, RespawnDelay, false);
}

void AMobaBaseCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	(void)DmgType;
	if (bDead)
	{
		return;
	}
	if (AttributeSet)
	{
		AttributeSet->SetHealth(0.f);
	}
	HandleDeath();
}

void AMobaBaseCharacter::Respawn()
{
	if (!HasAuthority())
	{
		return;
	}

	bDead = false;
	RespawnAtTime = 0.f;
	if (AttributeSet)
	{
		AttributeSet->SetHealth(AttributeSet->GetMaxHealth());
		AttributeSet->SetEnergy(AttributeSet->GetMaxEnergy());
	}
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Dead"), false));
	}

	if (AGameModeBase* GM = GetWorld()->GetAuthGameMode())
	{
		if (AActor* Start = GM->FindPlayerStart(GetController()))
		{
			TeleportTo(Start->GetActorLocation(), Start->GetActorRotation());
			SnapFacingToSpawn(Start->GetActorRotation());
		}
	}

	ClearAllStatus();
	ApplyAlivePresentation();
	bIgnoreShopRangeChanges = false;
	ScheduleShopRangeRefresh();
}

void AMobaBaseCharacter::PlayDeathAnimation()
{
	USkeletalMeshComponent* Skel = GetMesh();
	if (!Skel)
	{
		return;
	}

	Skel->SetHiddenInGame(false);
	if (!DeathAnimation)
	{
		Skel->SetHiddenInGame(true);
		return;
	}

	StopAnimMontage();
	Skel->PlayAnimation(DeathAnimation, false);
}

void AMobaBaseCharacter::RestoreSkeletalAnim()
{
	USkeletalMeshComponent* Skel = GetMesh();
	if (!Skel)
	{
		return;
	}

	Skel->Stop();
	Skel->SetHiddenInGame(false);
	Skel->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	if (UAnimInstance* Anim = Skel->GetAnimInstance())
	{
		Anim->Montage_Stop(0.f);
	}
}

void AMobaBaseCharacter::ApplyDeathPresentation()
{
	PlayDeathAnimation();
	if (HealthWidget)
	{
		HealthWidget->SetHiddenInGame(true);
	}
	RefreshCrosshairVisibility();
	bIgnoreShopRangeChanges = true;
	SetActorEnableCollision(false);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->DisableMovement();
		Movement->StopMovementImmediately();
	}
	if (IsLocallyControlled())
	{
		CreateRespawnHUD();
		if (RespawnHUD)
		{
			RespawnHUD->Refresh();
		}
	}
}

void AMobaBaseCharacter::ApplyAlivePresentation()
{
	RestoreSkeletalAnim();
	if (HealthWidget)
	{
		HealthWidget->SetHiddenInGame(IsLocallyControlled());
	}
	RefreshCrosshairVisibility();
	SetActorEnableCollision(true);
	UpdateOverlaps();
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
	if (RespawnHUD)
	{
		RespawnHUD->Refresh();
	}
}

void AMobaBaseCharacter::OnRep_Dead()
{
	if (bDead)
	{
		ApplyDeathPresentation();
	}
	else
	{
		ApplyAlivePresentation();
	}
}

void AMobaBaseCharacter::Move(const FInputActionValue& Value)
{
	if (bDead || bStunned || !Controller)
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Forward = Yaw.Vector();
	const FVector Right = (Yaw + FRotator(0.f, 90.f, 0.f)).Vector();

	AddMovementInput(Forward, Axis.Y);
	AddMovementInput(Right, Axis.X);
}

void AMobaBaseCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AMobaBaseCharacter::GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 SlotIndex)
{
	if (!AbilityClass || !AbilitySystemComponent)
	{
		return;
	}

	if (FGameplayAbilitySpec* Existing = AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass))
	{
		if (SlotIndex >= 0)
		{
			Existing->InputID = SlotIndex;
		}
		return;
	}

	AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, SlotIndex, this));
}

void AMobaBaseCharacter::NotifyNotEnoughEnergy()
{
	if (!IsLocallyControlled() || !AbilityHUD)
	{
		return;
	}
	AbilityHUD->ShowNotice(TEXT("Not enough energy"));
}

void AMobaBaseCharacter::PressAbility(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilityClass || bDead || bStunned)
	{
		return;
	}

	const UMobaGameplayAbility* CDO = Cast<UMobaGameplayAbility>(AbilityClass.GetDefaultObject());
	if (CDO && !HasEnergy(CDO->EnergyCost))
	{
		NotifyNotEnoughEnergy();
		return;
	}

	if (CDO && CDO->bHoldToAim)
	{
		BeginHoldAbility(AbilityClass);
		return;
	}

	CancelHoldAbility();

	if (CDO && CDO->ActivateEventTag.IsValid())
	{
		FGameplayEventData EventData;
		EventData.EventTag = CDO->ActivateEventTag;
		EventData.Instigator = this;
		EventData.Target = this;
		if (CDO->bSendMoveDirection)
		{
			EventData.TargetData = UMobaGameplayAbility::MakeDirectionTargetData(GetMoveDashDirection());
		}
		AbilitySystemComponent->HandleGameplayEvent(CDO->ActivateEventTag, &EventData);
		return;
	}

	AbilitySystemComponent->TryActivateAbilityByClass(AbilityClass);
}

void AMobaBaseCharacter::ReleaseAbility(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (HeldAbilityClass == AbilityClass)
	{
		ConfirmHoldAbility();
	}
}

void AMobaBaseCharacter::BeginHoldAbility(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (bDead || bStunned)
	{
		return;
	}

	const UMobaGameplayAbility* CDO = Cast<UMobaGameplayAbility>(AbilityClass.GetDefaultObject());
	if (!CDO)
	{
		return;
	}

	const FGameplayTag SlotCooldown = GetCooldownTagForAbilityClass(AbilityClass);
	if (SlotCooldown.IsValid() && AbilitySystemComponent
		&& AbilitySystemComponent->HasMatchingGameplayTag(SlotCooldown))
	{
		const FTimerHandle* Handle = CooldownHandles.Find(SlotCooldown);
		if (Handle && GetWorldTimerManager().IsTimerActive(*Handle))
		{
			return;
		}
		ClearCooldown(SlotCooldown);
	}

	if (!HasEnergy(CDO->EnergyCost))
	{
		NotifyNotEnoughEnergy();
		return;
	}

	CancelHoldAbility();
	CDO->BeginHold(this);
	HeldAbilityClass = AbilityClass;
}

void AMobaBaseCharacter::ConfirmHoldAbility()
{
	if (bDead || bStunned)
	{
		CancelHoldAbility();
		return;
	}

	const UMobaGameplayAbility* CDO = Cast<UMobaGameplayAbility>(HeldAbilityClass.GetDefaultObject());
	HeldAbilityClass = nullptr;
	if (CDO)
	{
		CDO->ConfirmHold(this);
	}
}

void AMobaBaseCharacter::CancelHoldAbility()
{
	const UMobaGameplayAbility* CDO = Cast<UMobaGameplayAbility>(HeldAbilityClass.GetDefaultObject());
	HeldAbilityClass = nullptr;
	if (CDO)
	{
		CDO->CancelHold(this);
	}
	ClearAimRing();
}

void AMobaBaseCharacter::SetAimRing(AActor* Ring)
{
	AimRing = Ring;
}

AActor* AMobaBaseCharacter::GetAimRing() const
{
	return AimRing;
}

void AMobaBaseCharacter::ClearAimRing()
{
	if (AimRing)
	{
		AimRing->Destroy();
		AimRing = nullptr;
	}
}

bool AMobaBaseCharacter::TryClaimVfx(FName Key)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (LastVfxKey == Key && Now - LastVfxTime < 0.25f)
	{
		return false;
	}
	LastVfxKey = Key;
	LastVfxTime = Now;
	return true;
}

bool AMobaBaseCharacter::TryConsumeAnimNotify(FGameplayTag Tag)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (LastAnimNotifyTag == Tag && Now - LastAnimNotifyTime < 0.08f)
	{
		return false;
	}
	LastAnimNotifyTag = Tag;
	LastAnimNotifyTime = Now;
	return true;
}

void AMobaBaseCharacter::MulticastSkillshotVfx_Implementation(
	TSubclassOf<AMobaProjectile> Class,
	FVector Start,
	FRotator Rotation,
	FVector Dir,
	float Speed,
	float Lifetime,
	float ExplodeAtZ)
{
	if (IsLocallyControlled() || !Class)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Instigator = this;

	if (AMobaProjectile* Bolt = World->SpawnActor<AMobaProjectile>(Class, Start, Rotation, Params))
	{
		Bolt->InitFlight(Dir, Speed, 0.f, Lifetime, true);
		if (ExplodeAtZ > -1.e8f)
		{
			Bolt->SetExplodeAtZ(ExplodeAtZ);
		}
	}
}

void AMobaBaseCharacter::MulticastGroundBlastVfx_Implementation(FVector Location, float Radius, float Lifetime)
{
	if (IsLocallyControlled())
	{
		return;
	}
	PlayGroundBlastDebug(Location, Radius, Lifetime);
}

void AMobaBaseCharacter::MulticastFireRingVfx_Implementation(float Radius, float Lifetime)
{
	if (IsLocallyControlled())
	{
		return;
	}
	PlayFireRingDebug(Radius, Lifetime);
}

void AMobaBaseCharacter::PlayAbilitySfx(USoundBase* Override, EMobaSfx Fallback, FVector Location)
{
	UMobaSfx::Play(this, Override, Fallback, Location);
	if (HasAuthority())
	{
		MulticastAbilitySfx(Override, Fallback, Location);
	}
}

void AMobaBaseCharacter::MulticastAbilitySfx_Implementation(USoundBase* Override, EMobaSfx Fallback, FVector Location)
{
	if (HasAuthority() || IsLocallyControlled())
	{
		return;
	}
	UMobaSfx::Play(this, Override, Fallback, Location);
}

void AMobaBaseCharacter::ServerConfirmGroundTarget_Implementation(TSubclassOf<UGameplayAbility> AbilityClass, FVector Location)
{
	if (!AbilitySystemComponent || !AbilityClass)
	{
		return;
	}
	const FGameplayTag CastingTag = FGameplayTag::RequestGameplayTag(FName("State.GroundCasting"), false);
	if (CastingTag.IsValid())
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(CastingTag);
	}
	SetPendingAbilityLocation(Location);
	if (!AbilitySystemComponent->TryActivateAbilityByClass(AbilityClass))
	{
		AbilitySystemComponent->TryActivateAbilityByClass(AbilityClass);
	}
}

void AMobaBaseCharacter::PressAbilitySlot(int32 SlotIndex)
{
	PressAbility(GetAbilitySlot(SlotIndex));
}

void AMobaBaseCharacter::ReleaseAbilitySlot(int32 SlotIndex)
{
	ReleaseAbility(GetAbilitySlot(SlotIndex));
}

int32 AMobaBaseCharacter::FindAbilitySlotByLabel(const TCHAR* Label) const
{
	for (int32 i = 0; i < AbilitySlots.Num(); ++i)
	{
		if (AbilitySlots[i].KeyLabel.Equals(Label, ESearchCase::IgnoreCase))
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void AMobaBaseCharacter::PressAbilityQ()
{
	const int32 Slot = FindAbilitySlotByLabel(TEXT("Q"));
	if (Slot != INDEX_NONE)
	{
		PressAbilitySlot(Slot);
	}
}

void AMobaBaseCharacter::PressAbilityE()
{
	const int32 Slot = FindAbilitySlotByLabel(TEXT("E"));
	if (Slot != INDEX_NONE)
	{
		PressAbilitySlot(Slot);
	}
}

void AMobaBaseCharacter::ReleaseAbilityE()
{
	const int32 Slot = FindAbilitySlotByLabel(TEXT("E"));
	if (Slot != INDEX_NONE)
	{
		ReleaseAbilitySlot(Slot);
	}
}

void AMobaBaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (Enhanced)
	{
		if (JumpAction)
		{
			Enhanced->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
			Enhanced->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}
		if (MoveAction)
		{
			Enhanced->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMobaBaseCharacter::Move);
		}
		if (LookAction)
		{
			Enhanced->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMobaBaseCharacter::Look);
		}
		EnsureAbilitySlots();
		for (int32 i = 0; i < AbilitySlots.Num(); ++i)
		{
			if (AbilitySlots[i].Input)
			{
				Enhanced->BindAction(AbilitySlots[i].Input, ETriggerEvent::Started, this, &AMobaBaseCharacter::PressAbilitySlot, i);
				Enhanced->BindAction(AbilitySlots[i].Input, ETriggerEvent::Completed, this, &AMobaBaseCharacter::ReleaseAbilitySlot, i);
				Enhanced->BindAction(AbilitySlots[i].Input, ETriggerEvent::Canceled, this, &AMobaBaseCharacter::ReleaseAbilitySlot, i);
			}
		}
		if (ShopInput)
		{
			Enhanced->BindAction(ShopInput, ETriggerEvent::Started, this, &AMobaBaseCharacter::ToggleShop);
		}
	}

	if (!ShopInput)
	{
		PlayerInputComponent->BindKey(EKeys::B, IE_Pressed, this, &AMobaBaseCharacter::ToggleShop);
	}
	EnsureAbilitySlots();
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AMobaBaseCharacter::PressAbilityQ);
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AMobaBaseCharacter::PressAbilityE);
	PlayerInputComponent->BindKey(EKeys::E, IE_Released, this, &AMobaBaseCharacter::ReleaseAbilityE);
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AMobaBaseCharacter::ToggleInventory);
	PlayerInputComponent->BindKey(EKeys::BackSpace, IE_Pressed, this, &AMobaBaseCharacter::ToggleSettings);

	APlayerController* PC = Cast<APlayerController>(GetController());
	ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	if (!LocalPlayer || !InputMapping)
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
	{
		Subsystem->AddMappingContext(InputMapping, 0);
	}
}
