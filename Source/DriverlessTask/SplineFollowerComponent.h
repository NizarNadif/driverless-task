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
#include "Kismet/KismetMathLibrary.h"

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Setup")
	AActor* TargetTrackActor;


	/* TUNING PARAMS (cm or seconds) */

	// look-ahead distance (cm) to start braking. It should be long
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Tuning")
	float BrakingLookAhead = 3000.0f;

	// MINIMUM steering look-ahead (for sharp turns)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Tuning")
	float MinLookAheadDistance = 800.0f;

	// MAXIMUM steering look-ahead (for straights)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Tuning")
	float MaxLookAheadDistance = 2000.0f;

	// how sharp a turn needs to be (dot product) to trigger full braking. 0.9 = gentle -> 0.5 = sharp
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Tuning")
	float BrakingSharpness = 0.8f;

	/* OBSTACLE AVOIDANCE PARAMS */

	// How far ahead the vehicle looks for obstacles (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Obstacle Avoidance")
	float ObstacleTraceDistance = 1000.0f;

	// radius of the sphere trace used for obstacle detection (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Obstacle Avoidance", meta = (ClampMin = "10.0"))
	float ObstacleTraceRadius = 220.0f;

	// angle (in degrees) of the avoidance probes (left and right)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Obstacle Avoidance")
	float AvoidanceProbeAngle = 30.0f;

	// How strongly the vehicle steers away from obstacles. Higher values = sharper turns.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Obstacle Avoidance")
	float AvoidanceStrength = 3.0f;


	/* STUCK RECOVERY PARAMS */

	// "stuck" time before reversing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Stuck")
	float MaxStuckTime = 2.0f;

	// reverse time to unstack (seconds)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Stuck")
	float UnstuckTime = 2.0f;

	/* TELEMETRY PARAMS */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Telemetry")
	int32 TelemetryDisplayIndex = 0;

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

	// State variable for recovery
	float StuckTime = 0.0f;
	float RecoverySteer = 0.0f;
	bool isPostRecovery = false;

	// Debug: trail line
	FVector PreviousLocation;

	void PrintTelemetry();
	bool HandleStuckState(float DeltaTime);
	void SeeDebugTrails(const FVector& VehicleLocation, const FVector& TargetLocation);
	bool FindSafeAvoidancePath(float& OutHitDistance, FVector& OutSafeDirection);
};
