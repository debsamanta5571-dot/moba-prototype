#pragma once

#include "CoreMinimal.h"
#include "MobaGameplayAbility.h"
#include "GA_MobaProjectile.generated.h"

class AMobaBaseCharacter;
class AMobaProjectile;
class UAnimMontage;
struct FGameplayEventData;

// Spawns a bolt. Local copy is cosmetic; server copy does the overlap.
UCLASS()
class MOBAPROJECT_API UGA_MobaProjectile : public UMobaGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MobaProjectile();
	virtual void PostLoad() override;

	UPROPERTY()
	TObjectPtr<UAnimMontage> SkillshotMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	TSubclassOf<AMobaProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Damage = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Speed = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Lifetime = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile",
		meta = (ToolTip = "Mesh socket to spawn from. Leave none to use the character location."))
	FName SpawnSocket;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile",
		meta = (ToolTip = "Added after the socket or character. X is forward, Y is right, Z is up."))
	FVector SpawnOffset = FVector(80.f, 0.f, 40.f);

protected:
	virtual bool PrepareCast(AMobaBaseCharacter* Character, const FGameplayEventData* TriggerEventData) override;
	virtual void OnCastNotify(FGameplayEventData Payload) override;

	void FireShot();
	void SpawnBolt(bool bCosmetic, bool bHideVisuals = false);
	FVector GetSpawnLocation(AMobaBaseCharacter* Character) const;

	bool bFiredThisCast = false;
};
