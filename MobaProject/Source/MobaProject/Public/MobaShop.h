#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MobaShop.generated.h"

class APawn;
class UCapsuleComponent;
class USceneComponent;

UCLASS(Blueprintable)
class MOBAPROJECT_API AMobaShop : public AActor
{
	GENERATED_BODY()

public:
	AMobaShop();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	int32 GetTeamId() const { return TeamID; }
	UCapsuleComponent* GetCapsule() const { return Capsule; }
	bool ContainsPawn(const APawn* Pawn) const;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moba")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Moba",
		meta = (ToolTip = "Only this team can use the shop and get shop regen. 1 and 2 match player teams."))
	int32 TeamID = 1;
};
