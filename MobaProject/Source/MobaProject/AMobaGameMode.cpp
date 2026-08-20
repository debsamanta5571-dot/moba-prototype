#include "AMobaGameMode.h"
#include "AMobaPlayerState.h"
#include "MobaBaseCharacter.h"

AAMobaGameMode::AAMobaGameMode()
{
	DefaultPawnClass = AMobaBaseCharacter::StaticClass();
	PlayerStateClass = AMobaPlayerState::StaticClass();
}

void AAMobaGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (AMobaPlayerState* PS = NewPlayer->GetPlayerState<AMobaPlayerState>())
	{
		PS->TeamID = ++NextTeamId;
	}
}
