#include "AnimNotify_AbilityEvent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "MobaBaseCharacter.h"

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
	if (!Tag.IsValid() || !MeshComp)
	{
		return;
	}

	UWorld* World = MeshComp->GetWorld();
	if (!World || World->WorldType == EWorldType::EditorPreview || World->WorldType == EWorldType::Editor)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner || !Owner->HasActorBegunPlay())
	{
		return;
	}

	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (MeshComp != Character->GetMesh())
		{
			return;
		}
	}

	if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Owner))
	{
		if (!Hero->TryConsumeAnimNotify(Tag))
		{
			return;
		}
	}

	if (Tag == FGameplayTag::RequestGameplayTag(FName("Event.Skillshot.Fire"), false)
		|| Tag == FGameplayTag::RequestGameplayTag(FName("Event.GroundTarget.Blast"), false))
	{
		return;
	}

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
