#include "MobaDescComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "MobaBaseCharacter.h"
#include "MobaDescHUD.h"
#include "MobaGameplayAbility.h"
#include "Misc/Char.h"

namespace
{
	FString FormatBoostedDamage(float Value)
	{
		const float Rounded = FMath::RoundToFloat(Value * 10.f) / 10.f;
		const int32 Whole = FMath::RoundToInt(Rounded);
		if (FMath::IsNearlyEqual(Rounded, static_cast<float>(Whole), 0.049f))
		{
			return FString::FromInt(Whole);
		}
		return FString::Printf(TEXT("%.1f"), Rounded);
	}

	FString ApplyDamageBoost(const FString& In, float Boost)
	{
		if (In.IsEmpty() || FMath::IsNearlyEqual(Boost, 1.f, 0.001f))
		{
			return In;
		}

		FString Out;
		const FString Lower = In.ToLower();
		int32 Cursor = 0;
		while (Cursor < In.Len())
		{
			const int32 DamageAt = Lower.Find(TEXT("damage"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
			if (DamageAt == INDEX_NONE)
			{
				Out += In.Mid(Cursor);
				break;
			}

			int32 NumEnd = DamageAt;
			while (NumEnd > Cursor && FChar::IsWhitespace(In[NumEnd - 1]))
			{
				--NumEnd;
			}

			int32 NumStart = NumEnd;
			bool bDot = false;
			while (NumStart > Cursor)
			{
				const TCHAR C = In[NumStart - 1];
				if (FChar::IsDigit(C))
				{
					--NumStart;
					continue;
				}
				if (C == TEXT('.') && !bDot)
				{
					bDot = true;
					--NumStart;
					continue;
				}
				break;
			}

			if (NumStart < NumEnd)
			{
				const float Base = FCString::Atof(*In.Mid(NumStart, NumEnd - NumStart));
				Out += In.Mid(Cursor, NumStart - Cursor);
				Out += FormatBoostedDamage(Base * Boost);
				Out += In.Mid(NumEnd, DamageAt + 6 - NumEnd);
				Cursor = DamageAt + 6;
			}
			else
			{
				Out += In.Mid(Cursor, DamageAt + 6 - Cursor);
				Cursor = DamageAt + 6;
			}
		}
		return Out;
	}
}

UMobaDescComponent::UMobaDescComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

TArray<FMobaAbilityDesc> UMobaDescComponent::GetLines() const
{
	const AMobaBaseCharacter* OwnerChar = Cast<AMobaBaseCharacter>(GetOwner());
	const float Boost = OwnerChar ? OwnerChar->GetDamageModifier() : 1.f;

	if (Abilities.Num() > 0)
	{
		TArray<FMobaAbilityDesc> Lines = Abilities;
		for (FMobaAbilityDesc& Line : Lines)
		{
			Line.Text = ApplyDamageBoost(Line.Text, Boost);
		}
		return Lines;
	}

	TArray<FMobaAbilityDesc> Fallback;
	if (!OwnerChar)
	{
		return Fallback;
	}

	const int32 Count = OwnerChar->GetAbilitySlotCount();
	for (int32 i = 0; i < Count; ++i)
	{
		FMobaAbilityDesc Line;
		UTexture2D* Icon = nullptr;
		float Remaining = 0.f;
		float Duration = 0.f;
		OwnerChar->GetAbilityHudInfo(i, Icon, Remaining, Duration);
		Line.Image = Icon;

		FString Label = OwnerChar->GetAbilityKeyLabel(i);
		if (Label.IsEmpty())
		{
			Label = FString::FromInt(i + 1);
		}
		FString AbilityName = TEXT("Ability");
		if (UClass* AbilityClass = OwnerChar->GetAbilitySlot(i).Get())
		{
			AbilityName = AbilityClass->GetName();
			AbilityName.RemoveFromEnd(TEXT("_C"));
			AbilityName.RemoveFromStart(TEXT("BP_GA_"));
			AbilityName.RemoveFromStart(TEXT("BP_"));
			AbilityName.RemoveFromStart(TEXT("GA_Moba"));
			AbilityName.RemoveFromStart(TEXT("GA_"));
		}
		Line.Text = Label + TEXT("  ") + AbilityName;
		Fallback.Add(Line);
	}
	return Fallback;
}

void UMobaDescComponent::EnsureOverlay()
{
	const UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return;
	}
	AMobaBaseCharacter* OwnerChar = Cast<AMobaBaseCharacter>(GetOwner());
	if (!OwnerChar || OwnerChar->IsActorBeingDestroyed() || !OwnerChar->IsLocallyControlled())
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(OwnerChar->GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (IsValid(Overlay))
	{
		return;
	}
	Overlay = CreateWidget<UMobaDescHUD>(PC, UMobaDescHUD::StaticClass());
	if (Overlay)
	{
		Overlay->SetDescComponent(this);
		Overlay->PlaceInViewport();
		Overlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMobaDescComponent::ToggleOverlay()
{
	SetOpen(!bOverlayOpen);
}

void UMobaDescComponent::SetOpen(bool bOpen)
{
	AMobaBaseCharacter* OwnerChar = Cast<AMobaBaseCharacter>(GetOwner());
	if (!OwnerChar || !OwnerChar->IsLocallyControlled())
	{
		return;
	}
	if (bOpen)
	{
		EnsureOverlay();
	}
	bOverlayOpen = bOpen;
	if (!Overlay)
	{
		return;
	}
	if (bOverlayOpen)
	{
		Overlay->RebuildList();
		Overlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		Overlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMobaDescComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Overlay)
	{
		Overlay->RemoveFromParent();
		Overlay = nullptr;
	}
	bOverlayOpen = false;
	Super::EndPlay(EndPlayReason);
}
