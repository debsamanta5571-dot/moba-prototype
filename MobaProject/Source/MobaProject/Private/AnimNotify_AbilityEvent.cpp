#include "AnimNotify_AbilityEvent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"

void UAnimNotify_AbilityEvent::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	FGameplayTag Tag = EventTag;
	if (!Tag.IsValid() && !DefaultEventTagName.IsNone())
	{
		Tag = FGameplayTag::RequestGameplayTag(DefaultEventTagName, false);
	}
	if (!Tag.IsValid())
	{
		return;
	}

	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Owner);
	UAbilitySystemComponent* ASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	FGameplayEventData Data;
	Data.EventTag = Tag;
	Data.Instigator = Owner;
	Data.Target = Owner;
	ASC->HandleGameplayEvent(Tag, &Data);
}
