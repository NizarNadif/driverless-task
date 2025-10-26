// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"

#include "ChaosVehicleMovementComponent.h"
#include "LandscapeSplinesComponent.h"
#include "Components/SplineComponent.h"
#include "LandscapeSplineActor.h"
#include "GameFramework/Pawn.h"

#include "SplineFollowerComponent.generated.h"

class USplineComponent;
class UChaosVehicleMovementComponent;
class APawn;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DRIVERLESSTASK_API USplineFollowerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	USplineFollowerComponent();

	// The spline track to follow (to set in the editor)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	AActor* TargetTrackActor;

	// Lookahead distance (cm) for steering
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float LookAheadDistance = 1500.0f;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY()
	APawn* OwnerPawn;

	UPROPERTY()
	UChaosVehicleMovementComponent* VehicleMovementComponent;
	
	UPROPERTY()
	USplineComponent* SplineToFollow;
};
