#include "AnimNotify_AbilityEvent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "MobaBaseCharacter.h"

namespace
{
	FGameplayTag ResolveAbilityNotifyTag(const FGameplayTag& Tag)
	{
		if (!Tag.IsValid())
		{
			return Tag;
		}
		if (Tag == FGameplayTag::RequestGameplayTag(FName("Event.Melee.Hit"), false))
		{
			return FGameplayTag::RequestGameplayTag(FName("Ability.1"), false);
		}
		if (Tag == FGameplayTag::RequestGameplayTag(FName("Event.Skillshot.Fire"), false))
		{
			return FGameplayTag::RequestGameplayTag(FName("Ability.2"), false);
		}
		return Tag;
	}
}

FString UAnimNotify_AbilityEvent::GetNotifyName_Implementation() const
{
	const FGameplayTag Tag = ResolveAbilityNotifyTag(EventTag);
	return Tag.IsValid() ? Tag.ToString() : TEXT("Ability Notify");
}

void UAnimNotify_AbilityEvent::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	const FGameplayTag Tag = ResolveAbilityNotifyTag(EventTag);
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
