#pragma once

#include "AnimNotify_AbilityEvent.h"
#include "CoreMinimal.h"
#include "AnimNotify_GroundTargetBlast.generated.h"

UCLASS()
class MOBAPROJECT_API UAnimNotify_GroundTargetBlast : public UAnimNotify_AbilityEvent
{
	GENERATED_BODY()

public:
	UAnimNotify_GroundTargetBlast();

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
