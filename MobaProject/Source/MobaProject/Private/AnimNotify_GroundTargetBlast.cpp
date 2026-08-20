#include "AnimNotify_GroundTargetBlast.h"

UAnimNotify_GroundTargetBlast::UAnimNotify_GroundTargetBlast()
{
	DefaultEventTagName = TEXT("Event.GroundTarget.Blast");
}

void UAnimNotify_GroundTargetBlast::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	EventTag = FGameplayTag::RequestGameplayTag(FName("Event.GroundTarget.Blast"), false);
	if (!EventTag.IsValid())
	{
		EventTag = FGameplayTag::RequestGameplayTag(FName("Event.GroundTarget"), false);
	}
	Super::Notify(MeshComp, Animation, EventReference);
}
