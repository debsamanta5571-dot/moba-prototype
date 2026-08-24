#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_AbilityEvent.generated.h"

/** One montage notify. Set EventTag to Ability.1, Ability.2, Ability.3, or Ability.4. */
UCLASS(meta = (DisplayName = "Ability Notify"))
class MOBAPROJECT_API UAnimNotify_AbilityEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	/** Gameplay tag to fire. Use Ability.1 through Ability.4. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notify", meta = (Categories = "Ability"))
	FGameplayTag EventTag;
};
