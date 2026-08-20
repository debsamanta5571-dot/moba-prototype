#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MobaGameInstance.generated.h"

UCLASS()
class MOBAPROJECT_API UMobaGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Moba")
	void HostGame();

	UFUNCTION(BlueprintCallable, Category = "Moba")
	void JoinGame(const FString& Address);

	virtual void LoadComplete(const float LoadTime, const FString& MapName) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Moba")
	FName ArenaMap = TEXT("/Game/Moba/Maps/MobaTestMap");

protected:
	void ReleaseMenuInput();
};
