#pragma once

#include "CoreMinimal.h"
#include "MobaGameplayAbility.h"
#include "GA_MobaTrace.generated.h"

class UAnimMontage;
struct FGameplayEventData;

// Sphere sweep along look yaw. Used for slash, punch, flamestrike — not a melee montage type.
UCLASS()
class MOBAPROJECT_API UGA_MobaTrace : public UMobaGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MobaTrace();
	virtual void PostLoad() override;

	UPROPERTY()
	TObjectPtr<UAnimMontage> MeleeMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
	float Damage = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
	float Range = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
	float Radius = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace", meta = (ClampMin = "1", ToolTip = "How many units this sweep can hit, closest first."))
	int32 MaxTargets = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace",
		meta = (ToolTip = "Draw a fire ring on the ground at slash reach. Use for flamestrike."))
	bool bShowRangeRing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace", meta = (EditCondition = "bShowRangeRing"))
	float RangeRingLifetime = 0.7f;

protected:
	virtual void OnCastStarted(AMobaBaseCharacter* Character) override;
	virtual void OnCastNotify(FGameplayEventData Payload) override;

	void TryHit();
	void ShowRangeRing();

	bool bHitThisSwing = false;
};
