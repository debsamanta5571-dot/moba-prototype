#pragma once

#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MobaGameplayAbility.generated.h"

class AMobaBaseCharacter;

UCLASS()
class MOBAPROJECT_API UMobaGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UMobaGameplayAbility();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	static FGameplayAbilityTargetDataHandle MakeDirectionTargetData(const FVector& Direction);
	static FGameplayAbilityTargetDataHandle MakeLocationTargetData(const FVector& Location);

	virtual void BeginHold(AMobaBaseCharacter* Avatar) const;
	virtual void ConfirmHold(AMobaBaseCharacter* Avatar) const;
	virtual void CancelHold(AMobaBaseCharacter* Avatar) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	FGameplayTag CooldownTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	float Cooldown = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	FGameplayTag ActivateEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	bool bSendMoveDirection = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	bool bHoldToAim = false;

protected:
	void ApplyMobaCooldown() const;
};
