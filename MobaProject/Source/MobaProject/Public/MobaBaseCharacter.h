#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "MobaBaseCharacter.generated.h"

class AActor;
class UAbilitySystemComponent;
class UCameraComponent;
class UGameplayAbility;
class UInputAction;
class UInputMappingContext;
class UMobaAttributeSet;
class USpringArmComponent;
class UWidgetComponent;
struct FInputActionValue;

UCLASS()
class MOBAPROJECT_API AMobaBaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMobaBaseCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	static bool ApplyMobaDamage(AActor* Target, float Amount, AActor* Instigator);

	void StartCooldown(FGameplayTag Tag, float Duration);
	void ClearCooldown(FGameplayTag Tag);
	FVector GetMoveDashDirection() const;
	void SetAimRing(AActor* Ring);
	AActor* GetAimRing() const;
	void ClearAimRing();
	void BeginPlantedAbility();
	void EndPlantedAbility();
	void HandleDeath();
	void Respawn();
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category = "Moba")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Moba")
	float GetMaxHealth() const;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void ApplyDeathPresentation();
	void ApplyAlivePresentation();

	UFUNCTION()
	void OnRep_Dead();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UMobaAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> HealthWidget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputMappingContext> InputMapping;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Abilities", meta = (FormerlySerializedAs = "MeleeAbility"))
	TSubclassOf<UGameplayAbility> Ability1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Abilities", meta = (FormerlySerializedAs = "MeleeAction"))
	TObjectPtr<UInputAction> Ability1Input;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Abilities", meta = (FormerlySerializedAs = "SkillshotAbility"))
	TSubclassOf<UGameplayAbility> Ability2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Abilities", meta = (FormerlySerializedAs = "SkillshotAction"))
	TObjectPtr<UInputAction> Ability2Input;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Abilities")
	TSubclassOf<UGameplayAbility> Ability3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Abilities")
	TObjectPtr<UInputAction> Ability3Input;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Abilities")
	TSubclassOf<UGameplayAbility> Ability4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Abilities")
	TObjectPtr<UInputAction> Ability4Input;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void PressAbility1();
	void PressAbility2();
	void PressAbility3();
	void PressAbility4();
	void ReleaseAbility1();
	void ReleaseAbility2();
	void ReleaseAbility3();
	void ReleaseAbility4();

	void GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass);
	void PressAbility(TSubclassOf<UGameplayAbility> AbilityClass);
	void ReleaseAbility(TSubclassOf<UGameplayAbility> AbilityClass);
	void BeginHoldAbility(TSubclassOf<UGameplayAbility> AbilityClass);
	void ConfirmHoldAbility();
	void CancelHoldAbility();

	TSubclassOf<UGameplayAbility> HeldAbilityClass;
	TObjectPtr<AActor> AimRing;
	TMap<FGameplayTag, FTimerHandle> CooldownHandles;
	int32 PlantedAbilityCount = 0;
	float DefaultMaxWalkSpeed = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float RespawnDelay = 5.f;

	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bDead = false;

	FTimerHandle RespawnTimer;
};
