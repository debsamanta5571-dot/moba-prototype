#include "MobaBaseCharacter.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AMobaPlayerState.h"
#include "AbilitySystemComponent.h"
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
#include "MobaHealthWidget.h"
#include "MobaMinion.h"
#include "Net/UnrealNetwork.h"

AMobaBaseCharacter::AMobaBaseCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

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
	HealthWidget->SetDrawSize(FVector2D(140.f, 18.f));
	HealthWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthWidget->SetWidgetClass(UMobaHealthWidget::StaticClass());
}

void AMobaBaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	
	if (HealthWidget)
	{
		if (UMobaHealthWidget* UI = Cast<UMobaHealthWidget>(HealthWidget->GetWidget()))
		{
			UI->SetOwnerCharacter(this);
		}
	}
}

void AMobaBaseCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	GrantAbility(Ability1);
	GrantAbility(Ability2);
	GrantAbility(Ability3);
	GrantAbility(Ability4);
}

void AMobaBaseCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
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

bool AMobaBaseCharacter::ApplyMobaDamage(AActor* Target, float Amount, AActor* Instigator)
{
	if (!IsValid(Target) || Amount <= 0.f || !Target->HasAuthority() || Target == Instigator)
	{
		return false;
	}

	const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Target);
	UAbilitySystemComponent* ASC = nullptr;
	if (ASI)
	{
		ASC = ASI->GetAbilitySystemComponent();
	}

	UMobaAttributeSet* Set = nullptr;
	if (ASC)
	{
		Set = const_cast<UMobaAttributeSet*>(ASC->GetSet<UMobaAttributeSet>());
	}
	if (!Set || Set->GetHealth() <= 0.f)
	{
		return false;
	}

	const int32 TargetTeam = MobaTeamIdOf(Target);
	const int32 InstigatorTeam = MobaTeamIdOf(Instigator);
	if (TargetTeam != 0 && TargetTeam == InstigatorTeam)
	{
		return false;
	}

	Set->SetHealth(FMath::Max(0.f, Set->GetHealth() - Amount));
	if (Set->GetHealth() <= 0.f)
	{
		if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Target))
		{
			Hero->HandleDeath();
		}
	}
	return true;
}

void AMobaBaseCharacter::StartCooldown(FGameplayTag Tag, float Duration)
{
	if (!Tag.IsValid() || Duration <= 0.f || !AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->AddLooseGameplayTag(Tag);
	FTimerHandle& Handle = CooldownHandles.FindOrAdd(Tag);
	GetWorldTimerManager().SetTimer(
		Handle,
		FTimerDelegate::CreateUObject(this, &AMobaBaseCharacter::ClearCooldown, Tag),
		Duration,
		false);
}

void AMobaBaseCharacter::ClearCooldown(FGameplayTag Tag)
{
	if (AbilitySystemComponent && Tag.IsValid())
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(Tag);
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

void AMobaBaseCharacter::BeginPlantedAbility()
{
	PlantedAbilityCount++;
	if (PlantedAbilityCount == 1)
	{
		GetCharacterMovement()->MaxWalkSpeed = DefaultMaxWalkSpeed * 0.5f;
	}
}

void AMobaBaseCharacter::EndPlantedAbility()
{
	if (PlantedAbilityCount <= 0)
	{
		return;
	}
	PlantedAbilityCount--;
	if (PlantedAbilityCount == 0)
	{
		GetCharacterMovement()->MaxWalkSpeed = DefaultMaxWalkSpeed;
	}
}

void AMobaBaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMobaBaseCharacter, bDead);
}

void AMobaBaseCharacter::HandleDeath()
{
	if (bDead || !HasAuthority())
	{
		return;
	}

	bDead = true;
	ApplyDeathPresentation();
	CancelHoldAbility();
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Dead"), false));
	}
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AMobaBaseCharacter::Respawn, RespawnDelay, false);
}

void AMobaBaseCharacter::Respawn()
{
	if (!HasAuthority())
	{
		return;
	}

	bDead = false;
	if (AttributeSet)
	{
		AttributeSet->SetHealth(AttributeSet->GetMaxHealth());
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

	ApplyAlivePresentation();
}

void AMobaBaseCharacter::ApplyDeathPresentation()
{
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		Skel->SetHiddenInGame(true);
	}
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
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		Skel->SetHiddenInGame(false);
	}
	if (HealthWidget)
	{
		HealthWidget->SetHiddenInGame(false);
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
	if (bDead)
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

void AMobaBaseCharacter::PressAbility(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilityClass || bDead)
	{
		return;
	}

	const UMobaGameplayAbility* CDO = Cast<UMobaGameplayAbility>(AbilityClass.GetDefaultObject());
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
	const UMobaGameplayAbility* CDO = Cast<UMobaGameplayAbility>(AbilityClass.GetDefaultObject());
	if (!CDO)
	{
		return;
	}

	if (CDO->CooldownTag.IsValid() && AbilitySystemComponent
		&& AbilitySystemComponent->HasMatchingGameplayTag(CDO->CooldownTag))
	{
		return;
	}

	CancelHoldAbility();
	CDO->BeginHold(this);
	HeldAbilityClass = AbilityClass;
}

void AMobaBaseCharacter::ConfirmHoldAbility()
{
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

	APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	Subsystem->AddMappingContext(InputMapping, 0);
}
