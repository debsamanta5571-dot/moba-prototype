#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "MobaCosmeticComponent.generated.h"

class AMobaBaseCharacter;

// Hats. Construction Script KeepWorld is wrong on SimulatedProxy (Head is still at the feet).
UCLASS(ClassGroup = (Moba), meta = (BlueprintSpawnableComponent))
class MOBAPROJECT_API UMobaCosmeticComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMobaCosmeticComponent();

	void TryAttachHat();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	AMobaBaseCharacter* GetHero() const;

	bool bHatAttached = false;
};
