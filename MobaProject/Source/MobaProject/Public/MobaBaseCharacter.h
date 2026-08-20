#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MobaBaseCharacter.generated.h"

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

	void StartMeleeCooldown(float Duration);
	void ClearMeleeCooldown();

	UFUNCTION(BlueprintPure, Category = "Moba")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Moba")
	float GetMaxHealth() const;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> MeleeAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Moba")
	TSubclassOf<UGameplayAbility> MeleeAbility;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Melee();

	FTimerHandle MeleeCooldownHandle;
};
