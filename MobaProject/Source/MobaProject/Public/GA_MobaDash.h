#pragma once

#include "CoreMinimal.h"
#include "MobaGameplayAbility.h"
#include "GA_MobaDash.generated.h"

class ACharacter;
class UAnimMontage;
struct FGameplayEventData;

UCLASS()
class MOBAPROJECT_API UGA_MobaDash : public UMobaGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MobaDash();
	virtual void PostLoad() override;

	UPROPERTY()
	TObjectPtr<UAnimMontage> DashMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	float DashStrength = 3500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	float DashDuration = 0.2f;

protected:
	virtual bool PrepareCast(AMobaBaseCharacter* Character, const FGameplayEventData* TriggerEventData) override;
	virtual void OnCastStarted(AMobaBaseCharacter* Character) override;

	UFUNCTION()
	void OnDashFinished();

	static FVector DirectionFromEvent(const FGameplayEventData* EventData, const ACharacter* FallbackCharacter);

	FVector PendingDashDir = FVector::ZeroVector;
};
