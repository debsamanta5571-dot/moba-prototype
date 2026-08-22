#include "MobaNetLibrary.h"
#include "GameFramework/PlayerState.h"

float UMobaNetLibrary::GetRoundTripPingSeconds(const APlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return 0.f;
	}
	return FMath::Max(0.f, PlayerState->GetPingInMilliseconds() * 0.001f);
}

float UMobaNetLibrary::CompensateCooldown(float Duration, float RoundTripPingSeconds)
{
	if (Duration <= 0.05f)
	{
		return Duration;
	}

	const float OneWay = FMath::Clamp(RoundTripPingSeconds * 0.5f, 0.f, 0.2f);
	return FMath::Max(Duration - OneWay, 0.05f);
}
