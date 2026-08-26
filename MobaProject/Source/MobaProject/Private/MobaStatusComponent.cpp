#include "MobaStatusComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "MobaAttributeSet.h"
#include "MobaBaseCharacter.h"
#include "MobaMinion.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UMobaStatusComponent::UMobaStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMobaStatusComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMobaStatusComponent, bStunned);
	DOREPLIFETIME(UMobaStatusComponent, SlowMul);
	DOREPLIFETIME(UMobaStatusComponent, HasteMul);
	DOREPLIFETIME(UMobaStatusComponent, StunUntilTime);
	DOREPLIFETIME(UMobaStatusComponent, SlowUntilTime);
	DOREPLIFETIME(UMobaStatusComponent, HasteUntilTime);
}

float UMobaStatusComponent::GetServerTimeSeconds() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}
		return World->GetTimeSeconds();
	}
	return 0.f;
}

bool UMobaStatusComponent::IsTimerExpired(float UntilTime) const
{
	return UntilTime <= KINDA_SMALL_NUMBER || GetServerTimeSeconds() >= UntilTime;
}

bool UMobaStatusComponent::IsOwnerDead() const
{
	if (const AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(GetOwner()))
	{
		return Hero->IsDead();
	}
	if (const AMobaMinion* Minion = Cast<AMobaMinion>(GetOwner()))
	{
		return Minion->IsDead();
	}
	return false;
}

float UMobaStatusComponent::GetOwnerCastSpeedMul() const
{
	if (const AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(GetOwner()))
	{
		return Hero->GetCastMoveSpeedMul();
	}
	return 1.f;
}

void UMobaStatusComponent::SetOwnerStatusTag(FName TagName, bool bEnabled)
{
	const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner());
	UAbilitySystemComponent* ASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName, false);
	if (!Tag.IsValid())
	{
		return;
	}
	if (bEnabled)
	{
		ASC->SetLooseGameplayTagCount(Tag, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
	else
	{
		ASC->SetLooseGameplayTagCount(Tag, 0, EGameplayTagReplicationState::TagAndCountToAll);
	}
}

void UMobaStatusComponent::NotifyOwnerStunned()
{
	if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(GetOwner()))
	{
		Hero->NotifyCrowdControlStun();
	}
}

void UMobaStatusComponent::ApplySpec(const FMobaEffectSpec& Spec)
{
	if (IsOwnerDead())
	{
		return;
	}

	UMobaAttributeSet* Set = const_cast<UMobaAttributeSet*>(UMobaAttributeSet::GetFromActor(GetOwner()));
	switch (Spec.Type)
	{
	case EMobaEffectType::Heal:
		if (Set && Spec.Magnitude > 0.f)
		{
			Set->SetHealth(FMath::Min(Set->GetMaxHealth(), Set->GetHealth() + Spec.Magnitude));
		}
		break;

	case EMobaEffectType::Stun:
		if (Spec.Duration > 0.f)
		{
			bStunned = true;
			StunUntilTime = GetServerTimeSeconds() + Spec.Duration;
			SetOwnerStatusTag("State.Stunned", true);
			GetWorld()->GetTimerManager().SetTimer(StunTimer, this, &UMobaStatusComponent::ClearStun, Spec.Duration, false);
			NotifyOwnerStunned();
			RefreshMoveSpeed();
		}
		break;

	case EMobaEffectType::Slow:
		if (Spec.Duration > 0.f)
		{
			MobaSlow::Add(SlowStack, Spec.Magnitude, Spec.Duration, GetServerTimeSeconds());
			RecalcSlow();
		}
		break;

	case EMobaEffectType::MoveSpeed:
		if (Spec.Duration > 0.f)
		{
			HasteMul = FMath::Max(Spec.Magnitude, 0.f);
			HasteUntilTime = GetServerTimeSeconds() + Spec.Duration;
			SetOwnerStatusTag("State.Hasted", true);
			GetWorld()->GetTimerManager().SetTimer(HasteTimer, this, &UMobaStatusComponent::ClearHaste, Spec.Duration, false);
			RefreshMoveSpeed();
		}
		break;

	default:
		break;
	}
}

void UMobaStatusComponent::RefreshMoveSpeed()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || IsOwnerDead())
	{
		return;
	}

	if (bStunned)
	{
		Movement->DisableMovement();
		Movement->StopMovementImmediately();
		return;
	}

	if (Movement->MovementMode == MOVE_None)
	{
		Movement->SetMovementMode(MOVE_Walking);
	}

	float Mul = SlowMul * HasteMul * GetOwnerCastSpeedMul();
	const UMobaAttributeSet* Set = UMobaAttributeSet::GetFromActor(GetOwner());
	float BaseSpeed = Set && Set->GetMoveSpeed() > 0.f ? Set->GetMoveSpeed() : Movement->MaxWalkSpeed;
	if (const AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(GetOwner()))
	{
		BaseSpeed = Set && Set->GetMoveSpeed() > 0.f ? Set->GetMoveSpeed() : Hero->GetDefaultWalkSpeed();
	}
	Movement->MaxWalkSpeed = BaseSpeed * Mul;
}

void UMobaStatusComponent::ClearStun()
{
	bStunned = false;
	StunUntilTime = 0.f;
	GetWorld()->GetTimerManager().ClearTimer(StunTimer);
	SetOwnerStatusTag("State.Stunned", false);
	RefreshMoveSpeed();
}

void UMobaStatusComponent::RecalcSlow()
{
	const float Now = GetServerTimeSeconds();
	MobaSlow::Prune(SlowStack, Now);
	if (SlowStack.Num() == 0)
	{
		ClearSlow();
		return;
	}

	SlowMul = MobaSlow::RemainingMul(SlowStack, Now);
	SlowUntilTime = MobaSlow::LastEndTime(SlowStack, Now);
	SetOwnerStatusTag("State.Slowed", true);
	const float Next = MobaSlow::NextEndTime(SlowStack, Now);
	GetWorld()->GetTimerManager().SetTimer(
		SlowTimer,
		this,
		&UMobaStatusComponent::RecalcSlow,
		FMath::Max(0.05f, Next - Now),
		false);
	RefreshMoveSpeed();
}

void UMobaStatusComponent::ClearSlow()
{
	SlowStack.Reset();
	SlowMul = 1.f;
	SlowUntilTime = 0.f;
	GetWorld()->GetTimerManager().ClearTimer(SlowTimer);
	SetOwnerStatusTag("State.Slowed", false);
	RefreshMoveSpeed();
}

void UMobaStatusComponent::ClearHaste()
{
	HasteMul = 1.f;
	HasteUntilTime = 0.f;
	GetWorld()->GetTimerManager().ClearTimer(HasteTimer);
	SetOwnerStatusTag("State.Hasted", false);
	RefreshMoveSpeed();
}

void UMobaStatusComponent::ClearAll()
{
	GetWorld()->GetTimerManager().ClearTimer(StunTimer);
	GetWorld()->GetTimerManager().ClearTimer(SlowTimer);
	GetWorld()->GetTimerManager().ClearTimer(HasteTimer);
	bStunned = false;
	SlowStack.Reset();
	SlowMul = 1.f;
	HasteMul = 1.f;
	StunUntilTime = 0.f;
	SlowUntilTime = 0.f;
	HasteUntilTime = 0.f;
	SetOwnerStatusTag("State.Stunned", false);
	SetOwnerStatusTag("State.Slowed", false);
	SetOwnerStatusTag("State.Hasted", false);
	RefreshMoveSpeed();
}

void UMobaStatusComponent::SanitizeTimedState()
{
	if (bStunned && IsTimerExpired(StunUntilTime))
	{
		ClearStun();
	}
	if (GetOwner() && GetOwner()->HasAuthority() && SlowStack.Num() > 0)
	{
		RecalcSlow();
	}
	else if (SlowMul < 0.99f && IsTimerExpired(SlowUntilTime))
	{
		ClearSlow();
	}
	if (HasteMul > 1.01f && IsTimerExpired(HasteUntilTime))
	{
		ClearHaste();
	}
}

void UMobaStatusComponent::OnRep_MoveStatus()
{
	SanitizeTimedState();
	if (bStunned)
	{
		NotifyOwnerStunned();
	}
	RefreshMoveSpeed();
}
