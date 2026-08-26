#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "MobaDescComponent.generated.h"

class AMobaBaseCharacter;
class UMobaDescHUD;
class UTexture2D;

USTRUCT(BlueprintType)
struct FMobaAbilityDesc
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	TObjectPtr<UTexture2D> Image;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba", meta = (MultiLine = "true"))
	FString Text;
};

UCLASS(ClassGroup = (Moba), meta = (BlueprintSpawnableComponent))
class MOBAPROJECT_API UMobaDescComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMobaDescComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	FString Title = TEXT("Abilities");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moba")
	TArray<FMobaAbilityDesc> Abilities;

	void ToggleOverlay();
	void SetOpen(bool bOpen);
	bool IsOpen() const { return bOverlayOpen; }
	TArray<FMobaAbilityDesc> GetLines() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void EnsureOverlay();

	UPROPERTY()
	TObjectPtr<UMobaDescHUD> Overlay;

	bool bOverlayOpen = false;
};
