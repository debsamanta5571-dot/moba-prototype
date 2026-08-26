#include "MobaCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AMobaPlayerState.h"
#include "MobaAttributeSet.h"
#include "MobaBaseCharacter.h"
#include "MobaMinion.h"
#include "MobaStatusComponent.h"
#include "MobaTower.h"

int32 MobaTeamIdOf(const AActor* Actor)
{
	if (!Actor)
	{
		return 0;
	}
	if (const AMobaPlayerState* PS = Cast<AMobaPlayerState>(Actor))
	{
		return PS->TeamID;
	}
	if (const AMobaMinion* Minion = Cast<AMobaMinion>(Actor))
	{
		return Minion->GetTeamId();
	}
	if (const AMobaTower* Tower = Cast<AMobaTower>(Actor))
	{
		return Tower->GetTeamId();
	}
	if (const AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Actor))
	{
		if (Hero->GetTeamId() != 0)
		{
			return Hero->GetTeamId();
		}
		if (const AMobaPlayerState* HeroPS = Hero->GetPlayerState<AMobaPlayerState>())
		{
			return HeroPS->TeamID;
		}
		return 0;
	}
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		if (const AMobaPlayerState* PawnPS = Pawn->GetPlayerState<AMobaPlayerState>())
		{
			return PawnPS->TeamID;
		}
	}
	return 0;
}

bool MobaIsEnemy(const AActor* A, const AActor* B)
{
	const int32 TeamA = MobaTeamIdOf(A);
	const int32 TeamB = MobaTeamIdOf(B);
	return TeamA != 0 && TeamB != 0 && TeamA != TeamB;
}

FLinearColor MobaAttitudeColor(const AActor* Viewer, const AActor* Target)
{
	if (MobaIsEnemy(Viewer, Target))
	{
		return FLinearColor(1.f, 0.42f, 0.42f, 1.f);
	}
	return FLinearColor(0.45f, 0.72f, 1.f, 1.f);
}

UMobaStatusComponent* UMobaCombatLibrary::GetStatus(AActor* Actor)
{
	if (const AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Actor))
	{
		return Hero->GetStatus();
	}
	if (const AMobaMinion* Minion = Cast<AMobaMinion>(Actor))
	{
		return Minion->GetStatus();
	}
	return Actor ? Actor->FindComponentByClass<UMobaStatusComponent>() : nullptr;
}

bool UMobaCombatLibrary::ApplyMobaDamage(AActor* Target, float Amount, AActor* Instigator)
{
	if (!IsValid(Target) || Amount <= 0.f || !Target->HasAuthority() || Target == Instigator)
	{
		return false; // never write Health on a simulated copy
	}

	UMobaAttributeSet* Set = const_cast<UMobaAttributeSet*>(UMobaAttributeSet::GetFromActor(Target));
	if (!Set || Set->GetHealth() <= 0.f)
	{
		return false;
	}

	if (!MobaIsEnemy(Instigator, Target))
	{
		return false;
	}

	float Incoming = Amount;
	const AMobaTower* AttackingTower = Cast<AMobaTower>(Instigator);
	if (AttackingTower && Cast<AMobaMinion>(Target))
	{
		// Towers chew a % of minion max HP so wave clear stays even as they level.
		Incoming = Set->GetMaxHealth() * FMath::Clamp(AttackingTower->GetMinionMaxHealthDamage(), 0.f, 1.f);
	}
	else
	{
		if (const UMobaAttributeSet* AttackerSet = UMobaAttributeSet::GetFromActor(Instigator))
		{
			Incoming *= FMath::Max(0.f, AttackerSet->GetDamageModifier());
		}
		Incoming *= 1.f - FMath::Clamp(Set->GetDamageResistance(), 0.f, 0.9f);
	}
	Incoming = FMath::Max(0.f, Incoming);

	Set->SetHealth(FMath::Max(0.f, Set->GetHealth() - Incoming));
	if (Incoming > 0.f)
	{
		const FVector NumberLoc = Target->GetActorLocation() + FVector(0.f, 0.f, 90.f);
		if (AMobaBaseCharacter* Dealer = Cast<AMobaBaseCharacter>(Instigator))
		{
			if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Target))
			{
				Hero->NotePlayerDamageFrom(Dealer); // last-hit credit window
				AMobaTower::NotifyHeroDamagedHero(Dealer, Hero);
			}
			else if (AMobaMinion* Minion = Cast<AMobaMinion>(Target))
			{
				Minion->NotePlayerDamageFrom(Dealer);
			}
			Dealer->NotifyDealtDamage(NumberLoc, Incoming);
		}
		if (AMobaBaseCharacter* HurtHero = Cast<AMobaBaseCharacter>(Target))
		{
			HurtHero->NotifyTakenDamage(NumberLoc, Incoming);
		}
		if (AMobaMinion* Minion = Cast<AMobaMinion>(Target))
		{
			Minion->NotifyDamagedBy(Instigator);
		}
	}
	if (Set->GetHealth() <= 0.f)
	{
		if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Target))
		{
			Hero->HandleDeath(Instigator);
		}
		else if (AMobaMinion* DeadMinion = Cast<AMobaMinion>(Target))
		{
			DeadMinion->HandleDeath(Instigator);
		}
		else if (AMobaTower* Tower = Cast<AMobaTower>(Target))
		{
			Tower->HandleDeath();
		}
	}
	return true;
}

void UMobaCombatLibrary::AwardKillGold(AActor* Victim, AActor* Killer)
{
	if (!IsValid(Victim))
	{
		return;
	}
	if (!Cast<AMobaBaseCharacter>(Victim) && !Cast<AMobaMinion>(Victim))
	{
		return;
	}

	const UMobaAttributeSet* VictimSet = UMobaAttributeSet::GetFromActor(Victim);
	if (!VictimSet)
	{
		return;
	}
	const float Gold = VictimSet->GetGoldOnKill();
	if (Gold <= 0.f)
	{
		return;
	}

	AMobaBaseCharacter* Recipient = Cast<AMobaBaseCharacter>(Killer);
	if (!Recipient)
	{
		if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Victim))
		{
			Recipient = Hero->GetPlayerKillCredit();
		}
		else if (AMobaMinion* Minion = Cast<AMobaMinion>(Victim))
		{
			Recipient = Minion->GetPlayerKillCredit();
		}
	}
	if (Recipient)
	{
		Recipient->AddGold(Gold);
		Recipient->NotifyGainedGold(Victim->GetActorLocation() + FVector(0.f, 0.f, 90.f), Gold);
	}
}

void UMobaCombatLibrary::ApplyMobaEffects(
	AActor* HitActor,
	AActor* Instigator,
	const TArray<FMobaEffectSpec>& Effects,
	EMobaEffectTarget Filter)
{
	for (const FMobaEffectSpec& Spec : Effects)
	{
		if (Spec.Target != Filter)
		{
			continue;
		}

		AActor* Target = (Spec.Target == EMobaEffectTarget::Self) ? Instigator : HitActor;
		if (!IsValid(Target) || !Target->HasAuthority())
		{
			continue;
		}

		if (AMobaBaseCharacter* Hero = Cast<AMobaBaseCharacter>(Target))
		{
			Hero->ApplyStatus(Spec);
			continue;
		}
		if (AMobaMinion* Minion = Cast<AMobaMinion>(Target))
		{
			Minion->ApplyStatus(Spec);
			continue;
		}
		if (Spec.Type == EMobaEffectType::Heal)
		{
			const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Target);
			UAbilitySystemComponent* ASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr;
			UMobaAttributeSet* Set = ASC ? const_cast<UMobaAttributeSet*>(ASC->GetSet<UMobaAttributeSet>()) : nullptr;
			if (Set)
			{
				Set->SetHealth(FMath::Min(Set->GetMaxHealth(), Set->GetHealth() + Spec.Magnitude));
			}
		}
	}
}
