#pragma once

#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MobaEffect.h"
#include "MobaSfx.h"
#include "MobaGameplayAbility.generated.h"

class AMobaBaseCharacter;
class USoundBase;
class UTexture2D;

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
	float EnergyCost = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	FGameplayTag ActivateEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	bool bSendMoveDirection = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	bool bHoldToAim = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	TArray<FMobaEffectSpec> Effects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability|SFX")
	TObjectPtr<USoundBase> CastSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability|SFX")
	TObjectPtr<USoundBase> HitSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability|SFX")
	EMobaSfx DefaultCastSfx = EMobaSfx::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability|SFX")
	EMobaSfx DefaultHitSfx = EMobaSfx::None;

	void PlayCastSfx() const;
	void PlayCastSfxAt(const FVector& Location) const;
	void PlayHitSfx(const FVector& Location) const;

	bool ApplyAbilityHit(AActor* Target, float InDamage, bool bApplySelfEffects = true) const;

protected:
	void ApplyMobaCooldown() const;
};
