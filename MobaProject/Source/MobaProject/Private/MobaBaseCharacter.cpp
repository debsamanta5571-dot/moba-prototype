#include "MobaBaseCharacter.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/DamageType.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AMobaPlayerState.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
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

#include "CollisionQueryParams.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "MobaCombatLibrary.h"
#include "MobaDescComponent.h"
#include "MobaGameInstance.h"
#include "MobaCrosshairHUD.h"
#include "GameFramework/GameStateBase.h"
#include "MobaGroundMarker.h"
#include "MobaHealthWidget.h"
#include "MobaBeamComponent.h"
#include "MobaCosmeticComponent.h"
#include "MobaHeroFxComponent.h"
#include "MobaHeroHudComponent.h"
#include "MobaMinion.h"
#include "MobaNetLibrary.h"
#include "MobaProjectile.h"
#include "MobaShopComponent.h"
#include "MobaStatusComponent.h"
#include "MobaTower.h"
#include "Net/UnrealNetwork.h"
#include "InputCoreTypes.h"

AMobaBaseCharacter::AMobaBaseCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // heroes don't body-block each other
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

	AbilityDesc = CreateDefaultSubobject<UMobaDescComponent>(TEXT("AbilityDesc"));
	Status = CreateDefaultSubobject<UMobaStatusComponent>(TEXT("Status"));
	HeroHud = CreateDefaultSubobject<UMobaHeroHudComponent>(TEXT("HeroHud"));
	HeroFx = CreateDefaultSubobject<UMobaHeroFxComponent>(TEXT("HeroFx"));
	Shop = CreateDefaultSubobject<UMobaShopComponent>(TEXT("Shop"));
	Beam = CreateDefaultSubobject<UMobaBeamComponent>(TEXT("Beam"));
	Cosmetics = CreateDefaultSubobject<UMobaCosmeticComponent>(TEXT("Cosmetics"));

	Hat = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hat"));
	Hat->SetupAttachment(GetMesh(), FName(TEXT("Head")));
	Hat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Hat->SetCanEverAffectNavigation(false);
	Hat->SetCastShadow(true);
	Hat->SetIsReplicated(false);
}

void AMobaBaseCharacter::PostLoad()
{
	Super::PostLoad();
	EnsureAbilitySlots();
	if (Shop && ShopOffers.Num() > 0)
	{
		Shop->AdoptLegacyOffers(ShopOffers);
		ShopOffers.Reset();
	}
}

void AMobaBaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	EnsureAbilitySlots();

	if (IsRunningDedicatedServer())
	{
		if (USkeletalMeshComponent* Skel = GetMesh())
		{
			Skel->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			Skel->bEnableUpdateRateOptimizations = false;
			Skel->SetComponentTickEnabled(true);
		}
	}

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

	if (IsLocallyControlled() && HeroHud)
	{
		HeroHud->TickLocal();
	}
	if (HeroFx)
	{
		HeroFx->TickFx();
	}
}

void AMobaBaseCharacter::StartGroundAimDebug(float Radius, float MaxRange)
{
	if (HeroFx)
	{
		HeroFx->StartGroundAimDebug(Radius, MaxRange);
	}
}

void AMobaBaseCharacter::StopGroundAimDebug()
{
	if (HeroFx)
	{
		HeroFx->StopGroundAimDebug();
	}
}

void AMobaBaseCharacter::PlayGroundBlastDebug(FVector Location, float Radius, float Lifetime)
{
	if (HeroFx)
	{
		HeroFx->PlayGroundBlastDebug(Location, Radius, Lifetime);
	}
}

void AMobaBaseCharacter::PlayFireRingDebug(float Radius, float Lifetime)
{
	if (HeroFx)
	{
		HeroFx->PlayFireRingDebug(Radius, Lifetime);
	}
}

void AMobaBaseCharacter::PlayBeamDebug(FVector Start, FVector End, float Radius, float Lifetime)
{
	if (HeroFx)
	{
		HeroFx->PlayBeamDebug(Start, End, Radius, Lifetime);
	}
}

void AMobaBaseCharacter::StopBeamDebug()
{
	if (HeroFx)
	{
		HeroFx->StopBeamDebug();
	}
}

void AMobaBaseCharacter::StartActiveBeam(FName Socket, const FVector& Offset, float Range, float Radius, float Lifetime, float TurnSpeed, float MaxPitch)
{
	if (Beam)
	{
		Beam->Start(Socket, Offset, Range, Radius, Lifetime, TurnSpeed, MaxPitch);
	}
}

void AMobaBaseCharacter::StopActiveBeam()
{
	if (Beam)
	{
		Beam->Stop();
	}
}

bool AMobaBaseCharacter::IsBeamActive() const
{
	return Beam && Beam->IsActive();
}

FVector AMobaBaseCharacter::GetActiveBeamStart() const
{
	return Beam ? Beam->GetStart() : FVector::ZeroVector;
}

FVector AMobaBaseCharacter::GetActiveBeamEnd() const
{
	return Beam ? Beam->GetEnd() : FVector::ZeroVector;
}

bool AMobaBaseCharacter::FindMobaFirePoint(FName Socket, FTransform& OutTransform) const
{
	if (Socket.IsNone())
	{
		return false;
	}

	const FString Wanted = Socket.ToString();
	TArray<FName> Candidates;
	Candidates.Add(Socket); // BPs name the socket differently per mesh, so we try a few fallbacks
	if (Wanted.Contains(TEXT("fire"), ESearchCase::IgnoreCase)
		|| Wanted.Contains(TEXT("muzzle"), ESearchCase::IgnoreCase))
	{
		Candidates.Add(FName(TEXT("weapon_r_muzzle")));
		Candidates.Add(FName(TEXT("weapon_l_muzzle")));
		Candidates.Add(FName(TEXT("ik_hand_gun")));
		Candidates.Add(FName(TEXT("HandGrip_R")));
		Candidates.Add(FName(TEXT("hand_r")));
	}

	TArray<USceneComponent*> SceneComps;
	GetComponents<USceneComponent>(SceneComps);
	for (const FName& Name : Candidates)
	{
		const FString NameStr = Name.ToString();
		for (USceneComponent* Comp : SceneComps)
		{
			if (!Comp)
			{
				continue;
			}
			if (Comp->GetFName() == Name || Comp->GetName().Equals(NameStr, ESearchCase::IgnoreCase))
			{
				OutTransform = Comp->GetComponentTransform();
				return true;
			}
		}
	}

	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (const FName& Name : Candidates)
	{
		for (UMeshComponent* MeshComp : Meshes)
		{
			if (MeshComp && MeshComp->DoesSocketExist(Name))
			{
				OutTransform = MeshComp->GetSocketTransform(Name);
				return true;
			}
		}
	}

	for (UMeshComponent* MeshComp : Meshes)
	{
		if (!MeshComp)
		{
			continue;
		}
		const TArray<FName> Sockets = MeshComp->GetAllSocketNames();
		for (const FName& Existing : Sockets)
		{
			if (Existing.ToString().Equals(Wanted, ESearchCase::IgnoreCase))
			{
				OutTransform = MeshComp->GetSocketTransform(Existing);
				return true;
			}
		}
	}

	return false;
}

void AMobaBaseCharacter::MulticastBeamVfx_Implementation(FVector Start, FVector End, float Radius, float Lifetime)
{
	if (IsLocallyControlled())
	{
		return;
	}
	PlayBeamDebug(Start, End, Radius, Lifetime);
}

void AMobaBaseCharacter::MulticastStopBeamVfx_Implementation()
{
	if (IsLocallyControlled())
	{
		return;
	}
	StopBeamDebug();
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

float AMobaBaseCharacter::GetDamageModifier() const
{
	return AttributeSet ? FMath::Max(0.f, AttributeSet->GetDamageModifier()) : 1.f;
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
	if (HeroFx)
	{
		HeroFx->SpawnFloatingNumber(Location, Amount, bGold);
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
	if (Shop)
	{
		Shop->RefreshRange();
	}
}

void AMobaBaseCharacter::ScheduleShopRangeRefresh()
{
	if (Shop)
	{
		Shop->ScheduleRangeRefresh();
	}
}

bool AMobaBaseCharacter::CanUseShop() const
{
	return Shop && Shop->CanUse();
}

bool AMobaBaseCharacter::CanBuyAnything() const
{
	return Shop && Shop->CanBuyAnything();
}

float AMobaBaseCharacter::GetShopOfferCost(int32 Index) const
{
	return Shop ? Shop->GetOfferCost(Index) : 0.f;
}

bool AMobaBaseCharacter::CanBuyShopOffer(int32 Index) const
{
	return Shop && Shop->CanBuy(Index);
}

void AMobaBaseCharacter::TryBuyShopOffer(int32 Index)
{
	if (Shop)
	{
		Shop->TryBuy(Index);
	}
}

const TArray<FMobaShopOffer>& AMobaBaseCharacter::GetShopOffers() const
{
	static const TArray<FMobaShopOffer> Empty;
	return Shop ? Shop->GetOffers() : Empty;
}

const TArray<FMobaShopOffer>& AMobaBaseCharacter::GetPurchasedOffers() const
{
	static const TArray<FMobaShopOffer> Empty;
	return Shop ? Shop->GetPurchased() : Empty;
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
	if (Shop && Shop->IsInRange())
	{
		HealthRate += Shop->GetHealthRegenBonus();
		EnergyRate += Shop->GetEnergyRegenBonus();
	}
	AttributeSet->SetHealth(FMath::Min(AttributeSet->GetMaxHealth(), AttributeSet->GetHealth() + HealthRate * Dt));
	AttributeSet->SetEnergy(FMath::Min(AttributeSet->GetMaxEnergy(), AttributeSet->GetEnergy() + EnergyRate * Dt));
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

bool AMobaBaseCharacter::IsStunned() const
{
	return Status && Status->IsStunned();
}

bool AMobaBaseCharacter::IsSlowed() const
{
	return Status && Status->IsSlowed();
}

bool AMobaBaseCharacter::IsShopOpen() const
{
	return HeroHud && HeroHud->IsShopOpen();
}

float AMobaBaseCharacter::GetCastMoveSpeedMul() const
{
	const bool bCasting = !IsTimerExpired(PlantedUntilTime)
		&& (!IsLocallyControlled() || PlantedAbilityCount > 0);
	return bCasting ? 0.5f : 1.f;
}

void AMobaBaseCharacter::NotifyCrowdControlStun()
{
	CancelHoldAbility();
	if (AbilitySystemComponent)
	{
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Beaming"), false));
		AbilitySystemComponent->CancelAbilities(&CancelTags);
	}
}

void AMobaBaseCharacter::ApplyStatus(const FMobaEffectSpec& Spec)
{
	if (Status)
	{
		Status->ApplySpec(Spec);
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
	if (Status)
	{
		Status->RefreshMoveSpeed();
	}
}

void AMobaBaseCharacter::ClearAllStatus()
{
	GetWorldTimerManager().ClearTimer(PlantedTimer);
	PlantedAbilityCount = 0;
	bPlanted = false;
	PlantedUntilTime = 0.f;
	if (Status)
	{
		Status->ClearAll();
	}
	else
	{
		RefreshMoveSpeed();
	}
}

bool AMobaBaseCharacter::IsTimerExpired(float UntilTime) const
{
	return UntilTime <= KINDA_SMALL_NUMBER || GetServerTimeSeconds() >= UntilTime;
}

void AMobaBaseCharacter::SanitizeTimedState()
{
	if (Status)
	{
		Status->SanitizeTimedState();
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
	if (IsStunned())
	{
		NotifyCrowdControlStun();
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
		// Simulated proxies started later by RTT. Shave ping off so the bar matches what they felt.
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
	if (AbilitySlots.Num() > 0)
	{
		return;
	}
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
	CommittedGroundTarget = Location;
	bHasPendingAbilityLocation = true;
	bHasCommittedGroundTarget = true;
}

FVector AMobaBaseCharacter::ConsumePendingAbilityLocation()
{
	bHasPendingAbilityLocation = false;
	return PendingAbilityLocation;
}

void AMobaBaseCharacter::SetPendingAbilityDirection(const FVector& Direction)
{
	PendingAbilityDirection = Direction;
	bHasPendingAbilityDirection = !Direction.IsNearlyZero();
}

FVector AMobaBaseCharacter::ConsumePendingAbilityDirection()
{
	bHasPendingAbilityDirection = false;
	return PendingAbilityDirection;
}

void AMobaBaseCharacter::ServerSetPendingAbilityDirection_Implementation(FVector Direction)
{
	SetPendingAbilityDirection(Direction);
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
	if (HeroHud)
	{
		HeroHud->CreateHud();
	}
}

void AMobaBaseCharacter::RefreshCrosshairVisibility()
{
	if (HeroHud)
	{
		HeroHud->RefreshCrosshairVisibility();
	}
}

void AMobaBaseCharacter::ToggleShop()
{
	if (HeroHud)
	{
		HeroHud->ToggleShop();
	}
}

void AMobaBaseCharacter::ToggleInventory()
{
	if (HeroHud)
	{
		HeroHud->ToggleInventory();
	}
}

void AMobaBaseCharacter::ToggleDesc()
{
	if (HeroHud)
	{
		HeroHud->ToggleDesc();
	}
}

void AMobaBaseCharacter::ToggleSettings()
{
	if (HeroHud)
	{
		HeroHud->ToggleSettings();
	}
}

void AMobaBaseCharacter::SetShopOpen(bool bOpen)
{
	if (HeroHud)
	{
		HeroHud->SetShopOpen(bOpen);
	}
}

void AMobaBaseCharacter::SetInventoryOpen(bool bOpen)
{
	if (HeroHud)
	{
		HeroHud->SetInventoryOpen(bOpen);
	}
}

void AMobaBaseCharacter::RefreshMenuInput()
{
	if (HeroHud)
	{
		HeroHud->RefreshMenuInput();
	}
}

void AMobaBaseCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilityDesc)
	{
		AbilityDesc->SetOpen(false);
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
	DOREPLIFETIME(AMobaBaseCharacter, bPlanted);
	DOREPLIFETIME(AMobaBaseCharacter, PlantedUntilTime);
	DOREPLIFETIME(AMobaBaseCharacter, CooldownSanity);
	DOREPLIFETIME(AMobaBaseCharacter, RespawnAtTime);
	DOREPLIFETIME_CONDITION(AMobaBaseCharacter, GroundBlastCueId, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AMobaBaseCharacter, GroundBlastRepLoc, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AMobaBaseCharacter, GroundBlastRepRadius, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AMobaBaseCharacter, GroundBlastRepLifetime, COND_SkipOwner);
}

void AMobaBaseCharacter::HandleDeath(AActor* Killer)
{
	if (bDead || !HasAuthority())
	{
		return;
	}

	UMobaCombatLibrary::AwardKillGold(this, Killer);
	bDead = true;
	ClearPlayerKillCredit();
	ClearAllStatus();
	CancelHoldAbility();
	StopActiveBeam();
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
	if (Shop)
	{
		Shop->SetIgnoreRangeChanges(false);
	}
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
	if (Shop)
	{
		Shop->SetIgnoreRangeChanges(true);
	}
	SetActorEnableCollision(false);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->DisableMovement();
		Movement->StopMovementImmediately();
	}
	if (HeroHud)
	{
		HeroHud->NotifyDeath();
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
	if (HeroHud)
	{
		HeroHud->NotifyAlive();
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
	if (bDead || IsStunned() || !Controller)
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
	if (HeroHud)
	{
		HeroHud->ShowEnergyNotice();
	}
}

void AMobaBaseCharacter::PressAbility(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilityClass || bDead || IsStunned())
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

	if (CDO && CDO->bSendMoveDirection)
	{
		const FVector Dir = GetMoveDashDirection();
		SetPendingAbilityDirection(Dir);
		if (!HasAuthority())
		{
			ServerSetPendingAbilityDirection(Dir);
		}
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
	if (bDead || IsStunned())
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
	if (bDead || IsStunned())
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

void AMobaBaseCharacter::NotifyGroundBlast(FVector Location, float Radius, float Lifetime)
{
	if (!HasAuthority())
	{
		return;
	}
	GroundBlastRepLoc = Location;
	GroundBlastRepRadius = FMath::Max(Radius, 80.f);
	GroundBlastRepLifetime = FMath::Max(Lifetime, 0.2f);
	++GroundBlastCueId;
}

void AMobaBaseCharacter::OnRep_GroundBlastCue()
{
	if (IsLocallyControlled() || GroundBlastCueId == 0)
	{
		return;
	}
	PlayGroundBlastDebug(GroundBlastRepLoc, GroundBlastRepRadius, GroundBlastRepLifetime);
}

bool AMobaBaseCharacter::PlayCastMontageIfNeeded(UAnimMontage* Montage, float Rate)
{
	if (!Montage)
	{
		return false;
	}
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		return false;
	}
	if (Anim->Montage_IsPlaying(Montage))
	{
		return true;
	}
	PlayAnimMontage(Montage, Rate);
	return true;
}

void AMobaBaseCharacter::MulticastPlayCastMontage_Implementation(UAnimMontage* Montage, float Rate)
{
	if (IsLocallyControlled())
	{
		return;
	}
	if (!PlayCastMontageIfNeeded(Montage, Rate))
	{
		PlaySlamMontage();
	}
}

void AMobaBaseCharacter::PlaySlamMontage()
{
	if (UAnimMontage* Montage = LoadObject<UAnimMontage>(
		nullptr,
		TEXT("/Game/Moba/Montages/Ground_Animmontage.Ground_Animmontage")))
	{
		PlayCastMontageIfNeeded(Montage, 1.f);
		return;
	}
	if (UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (UAnimSequence* Slam = LoadObject<UAnimSequence>(
			nullptr,
			TEXT("/Game/ParagonAnims/steelmanny/Ability/Steel_Ability_GroundSmah_Start.Steel_Ability_GroundSmah_Start")))
		{
			Anim->PlaySlotAnimationAsDynamicMontage(Slam, FName("DefaultSlot"), 0.08f, 0.25f, 1.f, 1);
		}
	}
}

void AMobaBaseCharacter::MulticastPlaySlam_Implementation()
{
	if (IsLocallyControlled())
	{
		return;
	}
	PlaySlamMontage();
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

	SetPendingAbilityLocation(Location);

	if (const FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass))
	{
		if (Spec->IsActive())
		{
			return;
		}
	}

	const FGameplayTag CastingTag = FGameplayTag::RequestGameplayTag(FName("State.GroundCasting"), false);
	if (CastingTag.IsValid())
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(CastingTag);
	}
	AbilitySystemComponent->TryActivateAbilityByClass(AbilityClass);
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
		if (InventoryInput)
		{
			Enhanced->BindAction(InventoryInput, ETriggerEvent::Started, this, &AMobaBaseCharacter::ToggleInventory);
		}
		if (DescInput)
		{
			Enhanced->BindAction(DescInput, ETriggerEvent::Started, this, &AMobaBaseCharacter::ToggleDesc);
		}
		if (SettingsInput)
		{
			Enhanced->BindAction(SettingsInput, ETriggerEvent::Started, this, &AMobaBaseCharacter::ToggleSettings);
		}
	}

	if (!ShopInput)
	{
		PlayerInputComponent->BindKey(EKeys::B, IE_Pressed, this, &AMobaBaseCharacter::ToggleShop);
	}
	if (!InventoryInput)
	{
		PlayerInputComponent->BindKey(EKeys::I, IE_Pressed, this, &AMobaBaseCharacter::ToggleInventory);
	}
	if (!DescInput)
	{
		PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AMobaBaseCharacter::ToggleDesc);
	}
	if (!SettingsInput)
	{
		PlayerInputComponent->BindKey(EKeys::BackSpace, IE_Pressed, this, &AMobaBaseCharacter::ToggleSettings);
		PlayerInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AMobaBaseCharacter::ToggleSettings);
	}
	EnsureAbilitySlots();
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AMobaBaseCharacter::PressAbilityQ);
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AMobaBaseCharacter::PressAbilityE);
	PlayerInputComponent->BindKey(EKeys::E, IE_Released, this, &AMobaBaseCharacter::ReleaseAbilityE);

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
