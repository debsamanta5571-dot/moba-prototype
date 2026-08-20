#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_AbilityEvent.generated.h"

UCLASS()
class MOBAPROJECT_API UAnimNotify_AbilityEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	FGameplayTag EventTag;

protected:
	FName DefaultEventTagName;
};
