#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "MobaEffect.h"
#include "MobaSfx.h"
#include "MobaShopTypes.h"
#include "MobaBaseCharacter.generated.h"

class AActor;
class AMobaGroundMarker;
class AMobaProjectile;
class UAbilitySystemComponent;
class UAnimMontage;
class UAnimationAsset;
class UCameraComponent;
class UGameplayAbility;
class UInputAction;
class UInputMappingContext;

USTRUCT(BlueprintType)
struct FMobaAbilityBind
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	TSubclassOf<UGameplayAbility> Ability;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	TObjectPtr<UInputAction> Input;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	FString KeyLabel;
};

class UMobaDescComponent;
class UMobaAttributeSet;
class UMobaStatusComponent;
class UMobaHeroHudComponent;
class UMobaHeroFxComponent;
class UMobaShopComponent;
class UMobaBeamComponent;
class UMobaCosmeticComponent;
class USoundBase;
class UTexture2D;
class USpringArmComponent;
class UWidgetComponent;
struct FInputActionValue;

USTRUCT()
struct FMobaTimerSanity
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	float UntilTime = 0.f;
};

UCLASS()
class MOBAPROJECT_API AMobaBaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMobaBaseCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	void NotePlayerDamageFrom(AMobaBaseCharacter* Player);
	AMobaBaseCharacter* GetPlayerKillCredit() const;
	void ClearPlayerKillCredit();
	void ApplyStatus(const FMobaEffectSpec& Spec);
	bool IsStunned() const;
	bool IsSlowed() const;
	UMobaStatusComponent* GetStatus() const { return Status; }
	UMobaHeroHudComponent* GetHeroHud() const { return HeroHud; }
	UMobaHeroFxComponent* GetHeroFx() const { return HeroFx; }
	UMobaShopComponent* GetShop() const { return Shop; }
	UMobaBeamComponent* GetBeam() const { return Beam; }
	UMobaCosmeticComponent* GetCosmetics() const { return Cosmetics; }
	UWidgetComponent* GetHealthWidget() const { return HealthWidget; }
	UWidgetComponent* GetCrosshair() const { return Crosshair; }
	UMobaDescComponent* GetAbilityDesc() const { return AbilityDesc; }
	float GetDefaultWalkSpeed() const { return DefaultMaxWalkSpeed; }
	float GetCastMoveSpeedMul() const;
	void NotifyCrowdControlStun();

	void StartCooldown(FGameplayTag Tag, float Duration);
	void ClearCooldown(FGameplayTag Tag);
	static FGameplayTag AbilitySlotTag(int32 SlotIndex);
	static FGameplayTag AbilitySlotCooldownTag(int32 SlotIndex) { return AbilitySlotTag(SlotIndex); }
	FGameplayTag GetAbilityTagForSlot(int32 SlotIndex) const;
	FGameplayTag GetCooldownTagForAbilityClass(TSubclassOf<UGameplayAbility> AbilityClass) const;
	void SetPendingAbilityLocation(const FVector& Location);
	FVector ConsumePendingAbilityLocation();
	bool HasPendingAbilityLocation() const { return bHasPendingAbilityLocation; }
	FVector GetCommittedGroundTarget() const { return CommittedGroundTarget; }
	bool HasCommittedGroundTarget() const { return bHasCommittedGroundTarget; }
	void PlaySlamMontage();
	bool PlayCastMontageIfNeeded(UAnimMontage* Montage, float Rate = 1.f);
	void NotifyGroundBlast(FVector Location, float Radius, float Lifetime);
	void SetPendingAbilityDirection(const FVector& Direction);
	FVector ConsumePendingAbilityDirection();
	bool HasPendingAbilityDirection() const { return bHasPendingAbilityDirection; }
	FVector GetMoveDashDirection() const;
	void SetAimRing(AActor* Ring);
	AActor* GetAimRing() const;
	void ClearAimRing();

	UFUNCTION(Server, Reliable)
	void ServerConfirmGroundTarget(TSubclassOf<UGameplayAbility> AbilityClass, FVector Location);

	UFUNCTION(Server, Reliable)
	void ServerSetPendingAbilityDirection(FVector Direction);

	void BeginPlantedAbility(float MaxDuration = 1.5f);
	void EndPlantedAbility();
	void HandleDeath(AActor* Killer = nullptr);
	void Respawn();
	bool IsDead() const { return bDead; }
	float GetRespawnRemaining() const;
	int32 GetTeamId() const { return TeamID; }
	void SetTeamId(int32 InTeam);
	void SyncTeamFromPlayerState();
	void SnapFacingToSpawn(const FRotator& SpawnRot);

	UFUNCTION(BlueprintPure, Category = "Moba")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Moba")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Moba")
	float GetEnergy() const;

	UFUNCTION(BlueprintPure, Category = "Moba")
	float GetMaxEnergy() const;

	UFUNCTION(BlueprintPure, Category = "Moba")
	float GetGold() const;

	UFUNCTION(BlueprintPure, Category = "Moba")
	float GetGoldOnKill() const;

	UFUNCTION(BlueprintPure, Category = "Moba")
	float GetDamageModifier() const;

	void AddGold(float Amount);
	void SpendGold(float Amount);

	bool HasEnergy(float Cost) const;
	void SpendEnergy(float Cost);
	void NotifyNotEnoughEnergy();

	void NotifyEnteredShop();
	void NotifyLeftShop();
	void RefreshShopRange();
	void ScheduleShopRangeRefresh();
	bool CanUseShop() const;
	bool IsShopOpen() const;
	bool CanBuyAnything() const;
	bool CanBuyShopOffer(int32 Index) const;
	float GetShopOfferCost(int32 Index) const;
	void TryBuyShopOffer(int32 Index);
	const TArray<FMobaShopOffer>& GetShopOffers() const;
	const TArray<FMobaShopOffer>& GetPurchasedOffers() const;
	void RefreshMoveSpeed();

	UFUNCTION(Server, Reliable)
	void ServerRequestPlayAgain();

	UFUNCTION(Server, Reliable)
	void ServerRequestReturnToMenu();

	void NotifyDealtDamage(FVector Location, float Amount);
	void NotifyTakenDamage(FVector Location, float Amount);
	void NotifyGainedGold(FVector Location, float Amount);
	bool TryClaimVfx(FName Key);
	bool TryConsumeAnimNotify(FGameplayTag Tag);
	void StartGroundAimDebug(float Radius, float MaxRange);
	void StopGroundAimDebug();
	void PlayGroundBlastDebug(FVector Location, float Radius, float Lifetime);
	void PlayFireRingDebug(float Radius, float Lifetime);
	void PlayBeamDebug(FVector Start, FVector End, float Radius, float Lifetime);
	void StopBeamDebug();
	void StartActiveBeam(FName Socket, const FVector& Offset, float Range, float Radius, float Lifetime, float TurnSpeed = 14.f, float MaxPitch = 70.f);
	void StopActiveBeam();
	bool IsBeamActive() const;
	FVector GetActiveBeamStart() const;
	FVector GetActiveBeamEnd() const;
	bool FindMobaFirePoint(FName Socket, FTransform& OutTransform) const;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBeamVfx(FVector Start, FVector End, float Radius, float Lifetime);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastStopBeamVfx();
	void PlayAbilitySfx(USoundBase* Override, EMobaSfx Fallback, FVector Location);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastAbilitySfx(USoundBase* Override, EMobaSfx Fallback, FVector Location);

	UFUNCTION(Client, Unreliable)
	void ClientShowDamageNumber(FVector Location, float Amount);

	UFUNCTION(Client, Unreliable)
	void ClientShowGoldNumber(FVector Location, float Amount);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSkillshotVfx(
		TSubclassOf<AMobaProjectile> Class,
		FVector Start,
		FRotator Rotation,
		FVector Dir,
		float Speed,
		float Lifetime,
		float ExplodeAtZ = -1.e9f);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastGroundBlastVfx(FVector Location, float Radius, float Lifetime);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayCastMontage(UAnimMontage* Montage, float Rate);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlaySlam();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFireRingVfx(float Radius, float Lifetime);

	TSubclassOf<UGameplayAbility> GetAbilitySlot(int32 Index) const;
	int32 GetAbilitySlotCount() const;
	FString GetAbilityKeyLabel(int32 Index) const;
	void GetAbilityHudInfo(int32 Index, UTexture2D*& OutIcon, float& OutRemaining, float& OutDuration) const;

protected:
	virtual void PostLoad() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void NotifyControllerChanged() override;
	virtual void PawnClientRestart() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;

	void CreateAbilityHUD();
	void RefreshCrosshairVisibility();
	void SpawnFloatingNumber(FVector Location, float Amount, bool bGold);
	float GetServerTimeSeconds() const;
	void ToggleShop();
	void ToggleInventory();
	void ToggleDesc();
	void ToggleSettings();
	void SetShopOpen(bool bOpen);
	void SetInventoryOpen(bool bOpen);
	void RefreshMenuInput();

	void TickRegen();
	void InitAttributeSet();

	void ApplyDeathPresentation();
	void ApplyAlivePresentation();
	void PlayDeathAnimation();
	void RestoreSkeletalAnim();

	UFUNCTION()
	void OnRep_Dead();

	UFUNCTION()
	void OnRep_MoveStatus();

	UFUNCTION()
	void OnRep_GroundBlastCue();

	void ClearAllStatus();
	void SetStatusTag(FName TagName, bool bEnabled);
	void SanitizeTimedState();
	void EndPlantedFromTimer();
	void UpsertCooldownSanity(FGameplayTag Tag, float UntilTime);
	void RemoveCooldownSanity(FGameplayTag Tag);
	bool IsTimerExpired(float UntilTime) const;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> Crosshair;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMobaDescComponent> AbilityDesc;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMobaStatusComponent> Status;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMobaHeroHudComponent> HeroHud;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMobaHeroFxComponent> HeroFx;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMobaShopComponent> Shop;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMobaBeamComponent> Beam;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMobaCosmeticComponent> Cosmetics;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputMappingContext> InputMapping;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	TArray<FMobaAbilityBind> AbilitySlots;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> ShopInput;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> InventoryInput;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> DescInput;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> SettingsInput;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void PressAbilitySlot(int32 SlotIndex);
	void ReleaseAbilitySlot(int32 SlotIndex);
	void PressAbilityQ();
	void PressAbilityE();
	void ReleaseAbilityE();
	int32 FindAbilitySlotByLabel(const TCHAR* Label) const;
	void EnsureAbilitySlots();

	void GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 SlotIndex = INDEX_NONE);
	void PressAbility(TSubclassOf<UGameplayAbility> AbilityClass);
	void ReleaseAbility(TSubclassOf<UGameplayAbility> AbilityClass);
	void BeginHoldAbility(TSubclassOf<UGameplayAbility> AbilityClass);
	void ConfirmHoldAbility();
	void CancelHoldAbility();

	TSubclassOf<UGameplayAbility> HeldAbilityClass;
	FVector PendingAbilityLocation = FVector::ZeroVector;
	FVector CommittedGroundTarget = FVector::ZeroVector;
	bool bHasPendingAbilityLocation = false;
	bool bHasCommittedGroundTarget = false;
	FVector PendingAbilityDirection = FVector::ZeroVector;
	bool bHasPendingAbilityDirection = false;
	TObjectPtr<AActor> AimRing;
	FName LastVfxKey;
	float LastVfxTime = -100.f;

	UPROPERTY(ReplicatedUsing = OnRep_GroundBlastCue)
	uint8 GroundBlastCueId = 0;

	UPROPERTY(Replicated)
	FVector GroundBlastRepLoc = FVector::ZeroVector;

	UPROPERTY(Replicated)
	float GroundBlastRepRadius = 250.f;

	UPROPERTY(Replicated)
	float GroundBlastRepLifetime = 0.55f;
	FGameplayTag LastAnimNotifyTag;
	float LastAnimNotifyTime = -100.f;
	TMap<FGameplayTag, FTimerHandle> CooldownHandles;
	TMap<FGameplayTag, float> CooldownDurations;
	TMap<FGameplayTag, float> CooldownEndTimes;

	UPROPERTY(Replicated)
	TArray<FMobaTimerSanity> CooldownSanity;

	int32 PlantedAbilityCount = 0;
	float DefaultMaxWalkSpeed = 500.f;

	UPROPERTY(ReplicatedUsing = OnRep_MoveStatus)
	bool bPlanted = false;

	UPROPERTY(ReplicatedUsing = OnRep_MoveStatus)
	float PlantedUntilTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	float RespawnDelay = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba", meta = (ClampMin = "0.0"))
	float PlayerKillCreditSeconds = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	TObjectPtr<UAnimationAsset> DeathAnimation;

	UPROPERTY(Replicated)
	float RespawnAtTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float DefaultMaxHealth = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float DefaultMaxEnergy = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float DefaultHealthRegen = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float DefaultEnergyRegen = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float StartingGold = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float DefaultGoldOnKill = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float DefaultGoldRegen = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float DefaultDamageModifier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes", meta = (DisplayName = "CDR", ClampMin = "0.0", ClampMax = "0.8"))
	float DefaultCooldownReduction = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float DefaultDamageResistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba|Attributes")
	float DefaultMoveSpeed = 500.f;

	UPROPERTY()
	TArray<FMobaShopOffer> ShopOffers;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Moba")
	int32 TeamID = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bDead = false;

	FTimerHandle RespawnTimer;
	FTimerHandle PlantedTimer;
	FTimerHandle RegenTimer;
	FTimerHandle PlayerKillCreditTimer;
	TWeakObjectPtr<AMobaBaseCharacter> LastPlayerDamager;
	float PlayerKillCreditUntilTime = 0.f;

};
