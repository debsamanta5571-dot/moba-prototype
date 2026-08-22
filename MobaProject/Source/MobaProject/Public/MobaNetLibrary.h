#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MobaNetLibrary.generated.h"

class APlayerState;

UCLASS()
class MOBAPROJECT_API UMobaNetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Moba|Net")
	static float GetRoundTripPingSeconds(const APlayerState* PlayerState);

	/** Shortens a server cooldown by one-way latency (half of round-trip ping). */
	UFUNCTION(BlueprintPure, Category = "Moba|Net")
	static float CompensateCooldown(float Duration, float RoundTripPingSeconds);
};
