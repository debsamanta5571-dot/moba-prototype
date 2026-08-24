#pragma once

#include "CoreMinimal.h"
#include "GA_MobaGroundTarget.h"
#include "GA_MobaGroundStrike.generated.h"

class AMobaProjectile;

UCLASS(Blueprintable)
class MOBAPROJECT_API UGA_MobaGroundStrike : public UGA_MobaGroundTarget
{
	GENERATED_BODY()

public:
	UGA_MobaGroundStrike();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundStrike")
	TSubclassOf<AMobaProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundStrike",
		meta = (ClampMin = "0.0"))
	float DropHeight = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundStrike",
		meta = (ClampMin = "1.0"))
	float Speed = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundStrike",
		meta = (ClampMin = "0.05"))
	float Lifetime = 2.f;

protected:
	virtual void ExecuteBlast() override;

	void SpawnDrop(bool bCosmetic, bool bHideVisuals);
};
