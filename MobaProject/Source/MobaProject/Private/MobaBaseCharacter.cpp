#include "MobaBaseCharacter.h"
#include "Abilities/GameplayAbility.h"
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
#include "AudioDevice.h"
#include "MobaAbilityHUD.h"
#include "MobaDamageNumber.h"
#include "MobaGoldHUD.h"
#include "MobaRespawnHUD.h"
#include "GameFramework/GameStateBase.h"
#include "DrawDebugHelpers.h"
#include "GA_MobaGroundTarget.h"
#include "MobaGroundMarker.h"
#include "MobaHealthWidget.h"
#include "MobaMinion.h"
#include "MobaNetLibrary.h"
#include "MobaProjectile.h"
#include "MobaShopHUD.h"
#include "MobaTower.h"
#include "Net/UnrealNetwork.h"
#include "InputCoreTypes.h"

AMobaBaseCharacter::AMobaBaseCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	DefaultMaxWalkSpeed = 500.f;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->bUsePawnControlRotation = false;

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
}

void AMobaBaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	bSfxMuted = false;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->ClearAudioListenerOverride();
	}

	InitAttributeSet();
	if (HealthWidget)
	{
		if (UMobaHealthWidget* UI = Cast<UMobaHealthWidget>(HealthWidget->GetWidget()))
		{
			UI->SetOwnerCharacter(this);
		}
	}

	CreateAbilityHUD();
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(RegenTimer, this, &AMobaBaseCharacter::TickRegen, 0.25f, true);
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
	if (IsLocallyControlled() && bSfxMuted)
	{
		ApplySfxListenerMute();
	}

	UWorld* World = GetWorld();
	if (!World || !IsLocallyControlled())
	{
		return;
	}

	if (bGroundAiming)
	{
		const FVector Loc = UGA_MobaGroundTarget::TraceGroundAim(this, GroundAimMaxRange);
		DrawDebugSphere(World, Loc, GroundAimRadius, 24, FColor::Cyan, false, -1.f, 0, 4.f);
		return;
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
	GroundBlastLoc = Location;
	GroundBlastRadius = Radius;
	GroundBlastDuration = FMath::Max(Lifetime, 0.05f);
	GroundBlastStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
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
	if (!bDead || RespawnAtTime <= 0.f)
	{
		return 0.f;
	}
	return FMath::Max(0.f, RespawnAtTime - GetServerTimeSeconds());
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

void AMobaBaseCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (!bSfxMuted)
	{
		if (APlayerController* PC = Cast<APlayerController>(NewController))
		{
			PC->ClearAudioListenerOverride();
		}
	}
	SyncTeamFromPlayerState();

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	GrantAbility(Ability1);
	GrantAbility(Ability2);
	GrantAbility(Ability3);
	GrantAbility(Ability4);
	CreateAbilityHUD();
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

void AMobaBaseCharacter::ClientShowDamageNumber_Implementation(FVector Location, float Amount)
{
	UWorld* World = GetWorld();
	if (!World || Amount <= 0.f)
	{
		return;
	}

	Location += FVector(FMath::FRandRange(-18.f, 18.f), FMath::FRandRange(-18.f, 18.f), FMath::FRandRange(-8.f, 16.f));

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
		Number->Init(Amount);
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
	++ShopRangeCount;
	bInShopRange = true;
}

void AMobaBaseCharacter::NotifyLeftShop()
{
	ShopRangeCount = FMath::Max(0, ShopRangeCount - 1);
	bInShopRange = ShopRangeCount > 0;
}

bool AMobaBaseCharacter::CanUseShop() const
{
	return bDead || bInShopRange;
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
	if (!HasAuthority() || bDead || !AttributeSet)
	{
		return;
	}

	const float Dt = 0.25f;
	AttributeSet->SetHealth(FMath::Min(AttributeSet->GetMaxHealth(), AttributeSet->GetHealth() + AttributeSet->GetHealthRegen() * Dt));
	AttributeSet->SetEnergy(FMath::Min(AttributeSet->GetMaxEnergy(), AttributeSet->GetEnergy() + AttributeSet->GetEnergyRegen() * Dt));
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
	if (const UMobaAttributeSet* AttackerSet = UMobaAttributeSet::GetFromActor(Instigator))
	{
		Incoming *= FMath::Max(0.f, AttackerSet->GetDamageModifier());
	}
	Incoming *= 1.f - FMath::Clamp(Set->GetDamageResistance(), 0.f, 0.9f);
	Incoming = FMath::Max(0.f, Incoming);

	Set->SetHealth(FMath::Max(0.f, Set->GetHealth() - Incoming));
	if (Incoming > 0.f)
	{
		if (AMobaMinion* Minion = Cast<AMobaMinion>(Target))
		{
			Minion->NotifyDamagedBy(Instigator);
		}
		if (AMobaBaseCharacter* Dealer = Cast<AMobaBaseCharacter>(Instigator))
		{
			Dealer->NotifyDealtDamage(Target->GetActorLocation() + FVector(0.f, 0.f, 90.f), Incoming);
		}
	}
	if (Set->GetHealth() <= 0.f)
	{
		if (AMobaBaseCharacter* Killer = Cast<AMobaBaseCharacter>(Instigator))
		{
			if (Cast<AMobaBaseCharacter>(Target) || Cast<AMobaMinion>(Target))
			{
				Killer->AddGold(Set->GetGoldOnKill());
			}
		}
		if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Target))
		{
			Hero->HandleDeath();
		}
		else if (AMobaMinion* DeadMinion = Cast<AMobaMinion>(Target))
		{
			DeadMinion->HandleDeath();
		}
		else if (AMobaTower* Tower = Cast<AMobaTower>(Target))
		{
			Tower->HandleDeath();
		}
	}
	return true;
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

TSubclassOf<UGameplayAbility> AMobaBaseCharacter::GetAbilitySlot(int32 Index) const
{
	switch (Index)
	{
	case 0: return Ability1;
	case 1: return Ability2;
	case 2: return Ability3;
	case 3: return Ability4;
	default: return nullptr;
	}
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
		if (Index >= 0 && Index < 4)
		{
			OutIcon = LoadObject<UTexture2D>(nullptr, IconPaths[Index]);
		}
	}
	OutDuration = CDO->Cooldown;
	if (AttributeSet)
	{
		const float Cdr = FMath::Clamp(AttributeSet->GetCooldownReduction(), 0.f, 0.8f);
		OutDuration = FMath::Max(CDO->Cooldown * (1.f - Cdr), 0.05f);
	}
	if (const float* Stored = CooldownDurations.Find(CDO->CooldownTag))
	{
		OutDuration = *Stored;
	}

	if (CDO->CooldownTag.IsValid())
	{
		if (const float* EndTime = CooldownEndTimes.Find(CDO->CooldownTag))
		{
			OutRemaining = FMath::Max(0.f, *EndTime - GetServerTimeSeconds());
		}
		else if (const FTimerHandle* Handle = CooldownHandles.Find(CDO->CooldownTag))
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
}

void AMobaBaseCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	CreateAbilityHUD();
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

	if (AbilityHUD)
	{
		AbilityHUD->PlaceInViewport();
		if (HealthWidget)
		{
			HealthWidget->SetHiddenInGame(true);
		}
		CreateGoldHUD();
		CreateShopHUD();
		CreateRespawnHUD();
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
	CreateGoldHUD();
	CreateShopHUD();
	CreateRespawnHUD();
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
		RespawnHUD->PlaceInViewport();
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

void AMobaBaseCharacter::ToggleShop()
{
	if (!IsLocallyControlled())
	{
		return;
	}
	CreateShopHUD();
	SetShopOpen(!bShopOpen);
}

void AMobaBaseCharacter::SetShopOpen(bool bOpen)
{
	bShopOpen = bOpen;
	if (ShopHUD)
	{
		ShopHUD->SetShopOpen(bOpen);
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	PC->bShowMouseCursor = bOpen;
	if (bOpen)
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
	DOREPLIFETIME(AMobaBaseCharacter, RespawnAtTime);
}

void AMobaBaseCharacter::HandleDeath()
{
	if (bDead || !HasAuthority())
	{
		return;
	}

	bDead = true;
	ClearAllStatus();
	CancelHoldAbility();
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAbilities();
		AbilitySystemComponent->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Dead"), false));
	}
	ApplyDeathPresentation();
	RespawnAtTime = GetServerTimeSeconds() + RespawnDelay;
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AMobaBaseCharacter::Respawn, RespawnDelay, false);
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
		}
	}

	ClearAllStatus();
	ApplyAlivePresentation();
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
	SetActorEnableCollision(false);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->DisableMovement();
		Movement->StopMovementImmediately();
	}
}

void AMobaBaseCharacter::ApplyAlivePresentation()
{
	RestoreSkeletalAnim();
	if (HealthWidget)
	{
		HealthWidget->SetHiddenInGame(IsLocallyControlled());
	}
	SetActorEnableCollision(true);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
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
	if (bDead || bStunned)
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

void AMobaBaseCharacter::GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (AbilityClass)
	{
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
	}
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

	if (CDO->CooldownTag.IsValid() && AbilitySystemComponent
		&& AbilitySystemComponent->HasMatchingGameplayTag(CDO->CooldownTag))
	{
		const FTimerHandle* Handle = CooldownHandles.Find(CDO->CooldownTag);
		if (Handle && GetWorldTimerManager().IsTimerActive(*Handle))
		{
			return;
		}
		ClearCooldown(CDO->CooldownTag);
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
	float Lifetime)
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
	if (GetNetMode() != NM_Client || IsLocallyControlled())
	{
		return;
	}
	UMobaSfx::Play(this, Override, Fallback, Location);
}

void AMobaBaseCharacter::ServerConfirmGroundTarget_Implementation(FVector Location)
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	const FGameplayTag ActivateTag = FGameplayTag::RequestGameplayTag(FName("Event.GroundTarget.Activate"), false);
	FGameplayEventData EventData;
	EventData.EventTag = ActivateTag;
	EventData.Instigator = this;
	EventData.Target = this;
	EventData.TargetData = UMobaGameplayAbility::MakeLocationTargetData(Location);
	AbilitySystemComponent->HandleGameplayEvent(ActivateTag, &EventData);
}

void AMobaBaseCharacter::PressAbility1()
{
	PressAbility(Ability1);
}

void AMobaBaseCharacter::PressAbility2()
{
	PressAbility(Ability2);
}

void AMobaBaseCharacter::PressAbility3()
{
	PressAbility(Ability3);
}

void AMobaBaseCharacter::PressAbility4()
{
	PressAbility(Ability4);
}

void AMobaBaseCharacter::ReleaseAbility1()
{
	ReleaseAbility(Ability1);
}

void AMobaBaseCharacter::ReleaseAbility2()
{
	ReleaseAbility(Ability2);
}

void AMobaBaseCharacter::ReleaseAbility3()
{
	ReleaseAbility(Ability3);
}

void AMobaBaseCharacter::ReleaseAbility4()
{
	ReleaseAbility(Ability4);
}

void AMobaBaseCharacter::ApplySfxListenerMute()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	if (bSfxMuted)
	{
		PC->SetAudioListenerOverride(nullptr, FVector(0.f, 0.f, -10000000.f), FRotator::ZeroRotator);
	}
	else
	{
		PC->ClearAudioListenerOverride();
	}
}

void AMobaBaseCharacter::ToggleMuteSfx()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	bSfxMuted = !bSfxMuted;
	ApplySfxListenerMute();
	if (bSfxMuted)
	{
		if (UWorld* World = GetWorld())
		{
			if (FAudioDevice* AudioDevice = World->GetAudioDeviceRaw())
			{
				AudioDevice->Flush(World);
			}
		}
	}
}

void AMobaBaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(PlayerInputComponent);
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
	if (Ability1Input)
	{
		Enhanced->BindAction(Ability1Input, ETriggerEvent::Started, this, &AMobaBaseCharacter::PressAbility1);
		Enhanced->BindAction(Ability1Input, ETriggerEvent::Completed, this, &AMobaBaseCharacter::ReleaseAbility1);
	}
	if (Ability2Input)
	{
		Enhanced->BindAction(Ability2Input, ETriggerEvent::Started, this, &AMobaBaseCharacter::PressAbility2);
		Enhanced->BindAction(Ability2Input, ETriggerEvent::Completed, this, &AMobaBaseCharacter::ReleaseAbility2);
	}
	if (Ability3Input)
	{
		Enhanced->BindAction(Ability3Input, ETriggerEvent::Started, this, &AMobaBaseCharacter::PressAbility3);
		Enhanced->BindAction(Ability3Input, ETriggerEvent::Completed, this, &AMobaBaseCharacter::ReleaseAbility3);
	}
	if (Ability4Input)
	{
		Enhanced->BindAction(Ability4Input, ETriggerEvent::Started, this, &AMobaBaseCharacter::PressAbility4);
		Enhanced->BindAction(Ability4Input, ETriggerEvent::Completed, this, &AMobaBaseCharacter::ReleaseAbility4);
	}
	if (ShopInput)
	{
		Enhanced->BindAction(ShopInput, ETriggerEvent::Started, this, &AMobaBaseCharacter::ToggleShop);
	}

	if (!ShopInput)
	{
		PlayerInputComponent->BindKey(EKeys::B, IE_Pressed, this, &AMobaBaseCharacter::ToggleShop);
	}
	PlayerInputComponent->BindKey(EKeys::L, IE_Pressed, this, &AMobaBaseCharacter::ToggleMuteSfx);

	APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	Subsystem->AddMappingContext(InputMapping, 0);
}
