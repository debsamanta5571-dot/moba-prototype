#include "AMobaPlayerState.h"
#include "Net/UnrealNetwork.h"

AMobaPlayerState::AMobaPlayerState()
{
}

void AMobaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMobaPlayerState, TeamID);
}

void AMobaPlayerState::OnRep_TeamId()
{
}
