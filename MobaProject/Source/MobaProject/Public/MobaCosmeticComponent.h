#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "MobaCosmeticComponent.generated.h"

class AMobaBaseCharacter;

// Strips leftover BP KeepWorld hats. Place the C++ Hat mesh in the hero Blueprint.
UCLASS(ClassGroup = (Moba), meta = (BlueprintSpawnableComponent))
class MOBAPROJECT_API UMobaCosmeticComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMobaCosmeticComponent();

	void TryAttachHat();

protected:
	virtual void BeginPlay() override;

	AMobaBaseCharacter* GetHero() const;
};
