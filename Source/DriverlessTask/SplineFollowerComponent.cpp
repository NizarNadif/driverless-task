// Fill out your copyright notice in the Description page of Project Settings.


#include "SplineFollowerComponent.h"
#include "Engine/Engine.h"
// circles to see projected path points
#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"

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

	PrintTelemetry();
	// if stuck, the handler has its own logic
	if (HandleStuckState(DeltaTime)) return;


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

	// map the curvature (1.0 to -1.0) to a "sharpness" factor (0.0 to 1.0)
	// BrakingSharpness indicates the sharpness that triggers full braking eg. the default is 0.8 = gentle
	float TurnSharpness = FMath::Clamp(1.0f - (Curvature / BrakingSharpness), 0.0f, 1.0f);
	
	// as the turn gets sharper, reduce look-ahead distance, to prevent cutting corners, and lower throttle + apply brakes
	// A linear interpolation is applied to smooth the transitions
	float PredictiveThrottle = FMath::Lerp(1.0f, 0.0f, TurnSharpness * 1.2f); // reduce throttle
	float CurvatureBrake = FMath::Lerp(0.0f, 1.0f, TurnSharpness * 1.5f); // apply brakes if the turn is sharp enough

	float SteeringLookAhead = FMath::Lerp(MaxLookAheadDistance, MinLookAheadDistance, TurnSharpness);

	/* STEERING */

	// Lookahead projection on the spline
	FVector TargetLocation = SplineToFollow->GetLocationAtDistanceAlongSpline(CurrentDistance + SteeringLookAhead, ESplineCoordinateSpace::World);

	// Calculate steering input
	const FVector DirectionToTarget = (TargetLocation - VehicleLocation).GetSafeNormal();
	const FVector CrossProduct = FVector::CrossProduct(VehicleForward, DirectionToTarget);
	float SplineSteering = FMath::Clamp(CrossProduct.Z, -1.0f, 1.0f);

	// 180° stall correction
	if (FMath::IsNearlyZero(SplineSteering, 0.01f))
	{
		if (FVector::DotProduct(VehicleForward, DirectionToTarget) < 0.0f) // if facing backwards
		{
			// determine which direction to turn
			FVector RightVector = OwnerPawn->GetActorRightVector();
			float RightDot = FVector::DotProduct(RightVector, DirectionToTarget);
			SplineSteering = (RightDot >= 0.0f) ? 1.0f : -1.0f; // turn right or left
		}
	}

	/* OBSTACLE AVOIDANCE */
	float AvoidanceFactor = 0.0f;
	float AvoidanceSteering = CalculateAvoidanceSteering(AvoidanceFactor);

	// Combine path following and obstacle avoidance
	float SteeringInput = SplineSteering;
	float AvoidanceBrake = CurvatureBrake;
	
	// proportionally blend steering and braking based on distance to obstacle, if any
	if (AvoidanceFactor > 0.1f) {

		// If avoidance factor is high, prioritize avoidance steering completely.
		// Blend only when avoidance factor is lower.
		float BlendAlpha = FMath::Clamp(AvoidanceFactor * 1.5f, 0.0f, 1.0f);
		SteeringInput = FMath::Lerp(SplineSteering, AvoidanceSteering * AvoidanceStrength, BlendAlpha);
		SteeringInput = FMath::Clamp(SteeringInput, -1.0f, 1.0f);

		AvoidanceBrake = FMath::Lerp(0.0f, 0.6f, FMath::Clamp((AvoidanceFactor - 0.7f) * 3.33f, 0.0f, 1.0f));
	}

	/* FINAL THROTTLE AND BRAKE */

	// reduce throttle based on final steering input
	float ReactiveThrottle = 1.0f - (FMath::Abs(SteeringInput) * 0.8f);
	// combine throttle factors (steering and predictive braking)
	// minimum throttle reduced based on avoidance factor
	float ThrottleInput = FMath::Min(PredictiveThrottle, ReactiveThrottle) * FMath::Lerp(1.0f, 0.4f, AvoidanceFactor);

	float BrakeInput = FMath::Max(CurvatureBrake, AvoidanceBrake);

	// Apply inputs to the vehicle movement component
	VehicleMovementComponent->SetSteeringInput(SteeringInput);
	VehicleMovementComponent->SetThrottleInput(ThrottleInput);
	VehicleMovementComponent->SetBrakeInput(BrakeInput);

	// SeeDebugTrails(VehicleLocation, TargetLocation);
}

float USplineFollowerComponent::CalculateAvoidanceSteering(float& OutAvoidanceFactor)
{
	OutAvoidanceFactor = 0.0f; // Default to no avoidance
	float CalculatedAvoidanceSteering = 0.0f;

	// Basic safety check
	if (ObstacleTraceDistance <= 0.0f || ObstacleTraceRadius <= 0.0f)
		return 0.0f;

	const FVector VehicleLocation = OwnerPawn->GetActorLocation();
	const FVector VehicleForward = OwnerPawn->GetActorForwardVector();

	// Trace start slightly in front and higher up to avoid ground hits
	const float StartForwardOffset = 150.0f;
	const float StartUpOffset = FMath::Max(ObstacleTraceRadius, 150.0f);
	const FVector TraceStart = VehicleLocation + (VehicleForward * StartForwardOffset) + (FVector::UpVector * StartUpOffset);
	
	const FVector TraceEnd = TraceStart + VehicleForward * ObstacleTraceDistance;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerPawn);

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(OwnerPawn);

	bool hit = UKismetSystemLibrary::SphereTraceSingleForObjects(
		GetWorld(),
		TraceStart,
		TraceEnd,
		ObstacleTraceRadius,
		ObjectTypes,
		false, // bTraceComplex
		ActorsToIgnore,
		EDrawDebugTrace::ForDuration, // Draw debug sphere
		HitResult,
		true, // bIgnoreSelf
		FLinearColor::Green,  // Trace color
		FLinearColor::Red, // Hit color
		0.2f // Debug duration
	);

	if (hit)
	{
		// Calculate strength based on inverse distance
		OutAvoidanceFactor = FMath::Clamp(1.0f - (HitResult.Distance / ObstacleTraceDistance), 0.0f, 1.0f);

		// Calculate steering direction based on hit location relative to forward vector
		FVector DirectionToHit = HitResult.ImpactPoint - TraceStart;
		DirectionToHit.Normalize();

		FVector Cross = FVector::CrossProduct(VehicleForward, DirectionToHit);

		if (FMath::Abs(Cross.Z) < 0.1f) // Near-center hit
			CalculatedAvoidanceSteering = 1.0f * OutAvoidanceFactor; // Default steer right
		else
			CalculatedAvoidanceSteering = -FMath::Sign(Cross.Z) * OutAvoidanceFactor; // Steer away

		// Clamp the steering result
		CalculatedAvoidanceSteering = FMath::Clamp(CalculatedAvoidanceSteering, -1.0f, 1.0f);
	}

	return CalculatedAvoidanceSteering;
}

void USplineFollowerComponent::PrintTelemetry()
{
	if (!GEngine) return;
	
	FString DebugMsg;

	/* STATE in key 1 */
	FString State = TEXT("NORMAL");

	if (StuckTime != 0.0f) {

		if (StuckTime > 0.0f) { // either stuck
			State = FString::Printf(TEXT("STUCK for %.2f / %.2f"), StuckTime, MaxStuckTime);
		}
		else { // or reversing
			State = FString::Printf(TEXT("REVERSING for %.2f / %.2f"), (UnstuckTime + StuckTime), UnstuckTime);
		}
	}

	DebugMsg = FString::Printf(TEXT("State: %s"), *State);
	GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Cyan, DebugMsg);

	/* SPEED in key 2 */
	float CurrentSpeed = FMath::Abs(VehicleMovementComponent->GetForwardSpeed()) * 0.036f; //km/h

	DebugMsg = FString::Printf(TEXT("Speed: %.1f km/h"), CurrentSpeed);
	GEngine->AddOnScreenDebugMessage(2, 0.0f, FColor::Yellow, DebugMsg);

	/* STEERING in key 3 */
	DebugMsg = FString::Printf(TEXT("Steering Input: %.2f"), VehicleMovementComponent->GetSteeringInput());
	GEngine->AddOnScreenDebugMessage(3, 0.0f, FColor::Green, DebugMsg);

	/* THROTTLE & BRAKE in key 4 */
	DebugMsg = FString::Printf(TEXT("Throttle: %.2f | Brake: %.2f"), VehicleMovementComponent->GetThrottleInput(), VehicleMovementComponent->GetBrakeInput());
	GEngine->AddOnScreenDebugMessage(4, 0.0f, FColor::Blue, DebugMsg);
}

bool USplineFollowerComponent::HandleStuckState(float DeltaTime)
{
	if (StuckTime < 0.0f) // reversing
	{
		VehicleMovementComponent->SetTargetGear(-1, true); // reverse
		VehicleMovementComponent->SetThrottleInput(1.0f);
		VehicleMovementComponent->SetSteeringInput(VehicleMovementComponent->GetSteeringInput());
		VehicleMovementComponent->SetBrakeInput(0.0f);

		StuckTime += DeltaTime;

		if (StuckTime >= 0.0f) {
			StuckTime = 0.0f;
			VehicleMovementComponent->SetTargetGear(1, true); // forward
		}

		return true;
	}

	// if we're practically still, update the stuck timer. Otherwise, reset it
	if (FMath::Abs(VehicleMovementComponent->GetForwardSpeed()) < 30.0f) // cm/s
		StuckTime += DeltaTime;
	else
		StuckTime = 0.0f;

	// if we're stuck for too long, initiate reversing
	if (StuckTime > MaxStuckTime)
	{
		StuckTime = -UnstuckTime;
		ReverseSteerDirection = (FMath::RandBool()) ? 1.0f : -1.0f; // random direction
		return true;
	}

	return false;
}

void USplineFollowerComponent::SeeDebugTrails(const FVector& VehicleLocation, const FVector &TargetLocation)
{
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
	PreviousTarget = TargetLocation;
}