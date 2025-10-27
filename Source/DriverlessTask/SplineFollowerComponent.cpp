// Fill out your copyright notice in the Description page of Project Settings.


#include "SplineFollowerComponent.h"
#include "Engine/Engine.h"
// circles to see projected path points
#include "DrawDebugHelpers.h"

// Sets default values for this component's properties
USplineFollowerComponent::USplineFollowerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

}


// Called when the game starts
void USplineFollowerComponent::BeginPlay()
{
	Super::BeginPlay();

	bool bSetupSuccess = true;

	// actor this component is attached to
	OwnerPawn = Cast<APawn>(GetOwner());

	// movement component of the vehicle
	if (OwnerPawn)
	{
		VehicleMovementComponent = Cast<UChaosVehicleMovementComponent>(OwnerPawn->GetMovementComponent());
		PreviousLocation = OwnerPawn->GetActorLocation();
		PreviousTarget = PreviousLocation;
	}
	else
	{
		bSetupSuccess = false;
		UE_LOG(LogTemp, Error, TEXT("SplineFollowerComponent: Owner is not a Pawn!"));
	}

	// spline component of the target track actor
	if (!TargetTrackActor)
	{
		bSetupSuccess = false;
		UE_LOG(LogTemp, Error, TEXT("SplineFollowerComponent: TargetTrackActor is NOT SET!"));
	}
	else
	{
		SplineToFollow = TargetTrackActor->FindComponentByClass<USplineComponent>();
		
		// if it's not a USPlineComponent, check if it's a LandscapeSplineActor, cast in case
		if (!SplineToFollow)
		{
			class ALandscapeSplineActor* LandscapeActor = Cast<ALandscapeSplineActor>(TargetTrackActor);
			if (LandscapeActor)
			{
				ULandscapeSplinesComponent* LandscapeSplineComponent = LandscapeActor->GetSplinesComponent();
				if (LandscapeSplineComponent)
				{
					// Create a new, temporary spline component to hold the path
					SplineToFollow = NewObject<USplineComponent>(this, TEXT("ConvertedLandscapeSpline"));
					SplineToFollow->RegisterComponent(); // Make it active

					// Copy the path data from the landscape spline into our new, empty spline
					LandscapeSplineComponent->CopyToSplineComponent(SplineToFollow);

					// check if the conversion actually worked
					if (SplineToFollow->GetNumberOfSplinePoints() < 2)
					{
						UE_LOG(LogTemp, Error, TEXT("SplineFollowerComponent: Failed to convert LandscapeSplineActor '%s' to SplineComponent!"), *TargetTrackActor->GetName());
						SplineToFollow = nullptr; // reset to null if conversion failed
					}
				}
			}
		}


		if (!SplineToFollow)
		{
			bSetupSuccess = false;
			UE_LOG(LogTemp, Error, TEXT("SplineFollowerComponent: TargetTrackActor '%s' does not have a SplineComponent!"), *TargetTrackActor->GetName());
		}
	}

	if (!bSetupSuccess || !VehicleMovementComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("SplineFollowerComponent: Setup Failed. Disabling tick."));
		SetComponentTickEnabled(false);
	}


}


// Called every frame
void USplineFollowerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Ensure all necessary components are valid
	if (!OwnerPawn || !VehicleMovementComponent || !SplineToFollow)
		return;

	if (GEngine)
	{
		float CurrentSpeed = FMath::Abs(VehicleMovementComponent->GetForwardSpeed()) * 0.036f; //km/h

		// Use a unique key (e.g., 1) to prevent spam
		FString DebugMsg = FString::Printf(TEXT("Speed: %.1f km/h"), CurrentSpeed);
		GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Yellow, DebugMsg);

		// Use key 2 for the state
		FString State = TEXT("NORMAL");

		if (StuckTime != 0.0f) {

			if (StuckTime > 0.0f) { // either stuck
				State = FString::Printf(TEXT("STUCK for %.2f / %.2f"), StuckTime, MaxStuckTime);
			}
			else { // or reversing
				State = FString::Printf(TEXT("REVERSING for %.2f / %.2f"), -1 * StuckTime, UnstuckTime);
			}
		}

		DebugMsg = FString::Printf(TEXT("State: %s"), *State);
		GEngine->AddOnScreenDebugMessage(2, 0.0f, FColor::Cyan, DebugMsg);
	}

	/* STUCK / RECOVERY */
	if (StuckTime < 0.0f)
	{
		// currently reversing
		VehicleMovementComponent->SetTargetGear(-1, true); // reverse
		VehicleMovementComponent->SetThrottleInput(1.0f);
		VehicleMovementComponent->SetSteeringInput(- VehicleMovementComponent->GetSteeringInput());
		VehicleMovementComponent->SetBrakeInput(0.0f);

		StuckTime += DeltaTime;

		if (StuckTime >= 0.0f) {
			StuckTime = 0.0f;
			VehicleMovementComponent->SetTargetGear(1, true); // forward
		}

		return;
	}

	// if we're practically still, update the stuck timer. Otherwise, reset it
	if (FMath::Abs(VehicleMovementComponent->GetForwardSpeed()) < 30.0f) // GetForwardSpeed() returns cm/s
		StuckTime += DeltaTime;
	else
		StuckTime = 0.0f;

	// if we're stuck for too long, initiate reversing
	if (StuckTime > MaxStuckTime)
	{
		StuckTime = -UnstuckTime;
		ReverseSteerDirection = (FMath::RandBool()) ? 1.0f : -1.0f; // random direction
		return;
	}


	/* PATH FOLLOWING */

	// Position of the vehicle
	FVector VehicleLocation = OwnerPawn->GetActorLocation();
	FVector VehicleForward = OwnerPawn->GetActorForwardVector();
	float SplineInputKey = SplineToFollow->FindInputKeyClosestToWorldLocation(VehicleLocation); // closest point of the spline to the vehicle
	float CurrentDistance = SplineToFollow->GetDistanceAlongSplineAtSplineInputKey(SplineInputKey);

	/* PREDICTIVE BRAKING (based on curve sharpness) */
	// direction (tangent) at a future point on the spline
	const FVector FutureTangent = SplineToFollow->GetTangentAtDistanceAlongSpline(CurrentDistance + BrakingLookAhead, ESplineCoordinateSpace::World);

	// diraction at the current point on the spline, right ahead of the vehicle
	const FVector CurrentTangent = SplineToFollow->GetTangentAtDistanceAlongSpline(CurrentDistance + 10.0f, ESplineCoordinateSpace::World);

	// dot product of the two directions, it indicates how sharp the curve is between current and future point
	// 1.0 = Perfectly , 0.0 = 90° turn , 1.0 = A 180° U-turn
	const float Curvature = FVector::DotProduct(CurrentTangent, FutureTangent);


	/* DYNAMIC LOOK-AHEAD & THROTTLE / BRAKE */
	float ThrottleInput = 1.0f;
	float BrakeInput = 0.0f;

	// map the curvature (1.0 to -1.0) to a "sharpness" factor (0.0 to 1.0)
	// BrakingSharpness indicates the sharpness that triggers full braking eg. the default is 0.8 = gentle
	float TurnSharpness = FMath::Clamp(1.0f - (Curvature / BrakingSharpness), 0.0f, 1.0f);
	
	// as the turn gets sharper, reduce look-ahead distance, to prevent cutting corners, and lower throttle + apply brakes
	// A linear interpolation is applied to smooth the transitions
	ThrottleInput = FMath::Lerp(1.0f, 0.0f, TurnSharpness * 1.2f); // reduce throttle
	BrakeInput = FMath::Lerp(0.0f, 1.0f, TurnSharpness * 1.5f); // apply brakes if the turn is sharp enough

	float SteeringLookAhead = FMath::Lerp(MaxLookAheadDistance, MinLookAheadDistance, TurnSharpness);

	/* STEERING */

	// Lookahead projection on the spline
	FVector TargetLocation = SplineToFollow->GetLocationAtDistanceAlongSpline(CurrentDistance + SteeringLookAhead, ESplineCoordinateSpace::World);

	// Calculate steering input
	const FVector DirectionToTarget = (TargetLocation - VehicleLocation).GetSafeNormal();
	const FVector CrossProduct = FVector::CrossProduct(VehicleForward, DirectionToTarget);
	float SteeringInput = FMath::Clamp(CrossProduct.Z, -1.0f, 1.0f);

	// Apply inputs to the vehicle movement component
	VehicleMovementComponent->SetSteeringInput(SteeringInput);
	VehicleMovementComponent->SetThrottleInput(ThrottleInput);
	VehicleMovementComponent->SetBrakeInput(BrakeInput);

	/* DEBUG */

	// Vehicle's trail line
	DrawDebugLine(
		GetWorld(), // context
		PreviousLocation, VehicleLocation, // start and end
		FColor::Cyan,
		false, // persistence
		5.0f, // lifetime (s)
		0, // depth priority, irrelevant here
		5.0f // thickness
	);

	if (GEngine)
	{
		FString DebugMsg = FString::Printf(TEXT("Target Move Dist Sq: %.2f"), FVector::DistSquared(PreviousTarget, TargetLocation));
		GEngine->AddOnScreenDebugMessage(4, 0.0f, FColor::Orange, DebugMsg);
	}

	// Target trail line
	if (FVector::DistSquared(PreviousTarget, TargetLocation) > 100.0f) {
		DrawDebugLine(
			GetWorld(),
			PreviousTarget, TargetLocation,
			FColor::Green, false, -1.0f, 0, 5.0f
		);
		PreviousTarget = TargetLocation;
	}

	// Target point sphere
	DrawDebugSphere(
		GetWorld(), TargetLocation, // context and location
		50.0f, // sphere's radius (cm)
		10, // # segments in the sphere eg. 3 = pyramid end
		// the following are the same params as line's (from color onward)
		FColor::Green, false, 0.0f, 0, 10.0f
	);

	// we keep track of previous locations for debug lines
	PreviousLocation = VehicleLocation;
	//PreviousTarget = TargetLocation;
}