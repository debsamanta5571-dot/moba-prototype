#include "MobaBaseCharacter.h"
#include "Abilities/GameplayAbility.h"
#include "AMobaPlayerState.h"
#include "AbilitySystemComponent.h"
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
#include "MobaHealthWidget.h"

AMobaBaseCharacter::AMobaBaseCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;

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
	if (MeleeAbility)
	{
		
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(MeleeAbility, 1, INDEX_NONE, this));
	}
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

	const APawn* TargetPawn = Cast<APawn>(Target);
	const APawn* InstigatorPawn = Cast<APawn>(Instigator);
	const AMobaPlayerState* TargetPS = nullptr;
	if (TargetPawn)
	{
		TargetPS = TargetPawn->GetPlayerState<AMobaPlayerState>();
	}
	const AMobaPlayerState* InstigatorPS = nullptr;
	if (InstigatorPawn)
	{
		InstigatorPS = InstigatorPawn->GetPlayerState<AMobaPlayerState>();
	}
	if (TargetPS && InstigatorPS && TargetPS->TeamID != 0 && TargetPS->TeamID == InstigatorPS->TeamID)
	{
		return false;
	}

	Set->SetHealth(FMath::Max(0.f, Set->GetHealth() - Amount));
	return true;
}

void AMobaBaseCharacter::StartMeleeCooldown(float Duration)
{
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("Cooldown.Melee"), false);
	AbilitySystemComponent->AddLooseGameplayTag(Tag);
	GetWorldTimerManager().SetTimer(MeleeCooldownHandle, this, &AMobaBaseCharacter::ClearMeleeCooldown, Duration, false);
}

void AMobaBaseCharacter::ClearMeleeCooldown()
{
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("Cooldown.Melee"), false);
	AbilitySystemComponent->RemoveLooseGameplayTag(Tag);
}

void AMobaBaseCharacter::Move(const FInputActionValue& Value)
{
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

void AMobaBaseCharacter::Melee()
{
	AbilitySystemComponent->TryActivateAbilityByClass(MeleeAbility);
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
	if (MeleeAction)
	{
		Enhanced->BindAction(MeleeAction, ETriggerEvent::Started, this, &AMobaBaseCharacter::Melee);
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	Subsystem->AddMappingContext(InputMapping, 0);
}
