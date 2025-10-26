// Fill out your copyright notice in the Description page of Project Settings.


#include "SplineFollowerComponent.h"

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
	{
		return;
	}

	// Position of the vehicle
	FVector VehicleLocation = OwnerPawn->GetActorLocation();
	float SplineInputKey = SplineToFollow->FindInputKeyClosestToWorldLocation(VehicleLocation); // closest point of the spline to the vehicle
	float CurrentDistance = SplineToFollow->GetDistanceAlongSplineAtSplineInputKey(SplineInputKey);

	// Lookahead projection on the spline
	float TargetDistance = CurrentDistance + LookAheadDistance;
	FVector TargetLocation = SplineToFollow->GetLocationAtDistanceAlongSpline(TargetDistance, ESplineCoordinateSpace::World);

	// Calculate steering input
	const FVector DirectionToTarget = (TargetLocation - VehicleLocation).GetSafeNormal();
	const FVector VehicleForward = OwnerPawn->GetActorForwardVector();
	const FVector CrossProduct = FVector::CrossProduct(VehicleForward, DirectionToTarget);
	float SteeringInput = FMath::Clamp(CrossProduct.Z, -1.0f, 1.0f);

	// Throttle input
	float ThrottleInput = 1.0f - FMath::Abs(SteeringInput) * 0.8f;

	// Apply inputs to the vehicle movement component
	VehicleMovementComponent->SetSteeringInput(SteeringInput);
	VehicleMovementComponent->SetThrottleInput(ThrottleInput);
	VehicleMovementComponent->SetBrakeInput(0.0f);
}