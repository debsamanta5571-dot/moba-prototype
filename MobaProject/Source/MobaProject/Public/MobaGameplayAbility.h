#pragma once

#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MobaEffect.h"
#include "MobaSfx.h"
#include "MobaGameplayAbility.generated.h"

class AMobaBaseCharacter;
class UAnimMontage;
class USoundBase;
class UTexture2D;

// Shared cast path: predicted activate, montage notify, cooldown on the ASC (not this object — CanActivate can run on the CDO).
UCLASS()
class MOBAPROJECT_API UMobaGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UMobaGameplayAbility();
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	static FGameplayAbilityTargetDataHandle MakeDirectionTargetData(const FVector& Direction);
	static FGameplayAbilityTargetDataHandle MakeLocationTargetData(const FVector& Location);

	virtual void BeginHold(AMobaBaseCharacter* Avatar) const;
	virtual void ConfirmHold(AMobaBaseCharacter* Avatar) const;
	virtual void CancelHold(AMobaBaseCharacter* Avatar) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	float Cooldown = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	float EnergyCost = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability",
		meta = (ToolTip = "Played on activate. Payload fires on the slot notify (Ability.1-4), or immediately if empty."))
	TObjectPtr<UAnimMontage> CastMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	bool bSendMoveDirection = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	bool bHoldToAim = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability",
		meta = (ToolTip = "Slow the caster while the montage / cast is active."))
	bool bPlantOnCast = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	float PlantDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability",
		meta = (ToolTip = "If on, the ability ends when the montage finishes, or immediately when there is no montage. Dash and beam turn this off."))
	bool bEndOnMontage = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	bool bPlayCastSfxOnStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	TArray<FMobaEffectSpec> Effects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability",
		meta = (ToolTip = "If off, this ability ignores towers. Bolts still collide with the building."))
	bool bCanDamageTowers = true;

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
	FGameplayTag ResolveCooldownTag(const AActor* Avatar) const;
	FGameplayTag ResolveNotifyTag(const AActor* Avatar) const;
	UAnimMontage* GetCastMontage() const { return CastMontage; }

protected:
	void ApplyMobaCooldown() const;
	void AdoptLegacyMontage(UAnimMontage* Legacy);

	virtual bool PrepareCast(AMobaBaseCharacter* Character, const FGameplayEventData* TriggerEventData);
	virtual void OnCastStarted(AMobaBaseCharacter* Character);
	virtual void OnCastNotify(FGameplayEventData Payload);
	virtual void OnCastMontageDone();
	virtual float GetPlantDuration() const { return PlantDuration; }

	virtual void StartCastMontage();
	void EndCastPlant();
	bool ShouldUseMontageFallback() const;
	void ScheduleMontageFallback(UAnimMontage* Montage);

	UFUNCTION()
	void HandleCastNotify(FGameplayEventData Payload);

	UFUNCTION()
	void HandleCastNotifyFallback();

	UFUNCTION()
	void HandleCastMontageDone();

	bool bCastNotifyFired = false;
	bool bCastMontageDone = false;
	bool bPlantedThisCast = false;
	FTimerHandle NotifyFallbackTimer;
	FTimerHandle MontageFallbackTimer;
};
