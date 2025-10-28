// Fill out your copyright notice in the Description page of Project Settings.


#include "ObstacleSpawnerActor.h"
#include "LandscapeSplineActor.h"
#include "LandscapeSplinesComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Engine.h"

// Sets default values
AObstacleSpawnerActor::AObstacleSpawnerActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	PathSplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("InternalPathSpline"));

}

// Called when the game starts or when spawned
void AObstacleSpawnerActor::BeginPlay()
{
	Super::BeginPlay();
	SpawnObstacles();
}

void AObstacleSpawnerActor::SpawnObstacles()
{
	if (!CheckRequirements()) return;

	/* SPAWN LOGIC */
	World = GetWorld();
	if (!World) return;

	const float SplineLength = PathSplineComponent->GetSplineLength();
	SpawnedObstaclesLocations.Empty();

	// attempt to place the desired number of obstacles, with cap number of attempts
	int ObstaclesPlaced = 0;
	const int MaxAttempts = NumberOfObstacles * 10;
	int attempts;

	for (attempts = 0; attempts < MaxAttempts && ObstaclesPlaced < NumberOfObstacles; attempts++)
	{
		FVector SpawnLocation = GetRandomPointAlongSpline(SplineLength);

		bool tooClose = false;
		for (const FVector& ExistingLocation : SpawnedObstaclesLocations)
		{
			if (FVector::DistSquared(ExistingLocation, SpawnLocation) < FMath::Square(MinDistanceBetweenObstacles))
			{
				tooClose = true;
				break;
			}
		}

		if (tooClose) continue;

		if (CreateObstacle(SpawnLocation))
			ObstaclesPlaced++;
	}

	UE_LOG(LogTemp, Log, TEXT("Placed %d obstacles along Landscape Spline after %d attempts."), ObstaclesPlaced, attempts);
}

bool AObstacleSpawnerActor::CheckRequirements()
{
	// Initial checks and path's conversion
	if (!TrackSplineActor || !ObstacleMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("ObstacleSpawnerActor: Missing TrackSplineActor or ObstacleMesh reference."));
		return false;
	}

	if (NumberOfObstacles <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ObstacleSpawnerActor: NumberOfObstacles must be greater than zero."));
		return false;
	}

	ULandscapeSplinesComponent* LandscapeSplineComp = TrackSplineActor->GetSplinesComponent();
	if (!LandscapeSplineComp || LandscapeSplineComp->GetControlPoints().Num() < 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("ObstacleSpawnerActor: Unable to get LandscapeSplinesComponent from TrackSplineActor."));
	}

	// Convert the landscape spline to an internal USplineComponent
	PathSplineComponent->ClearSplinePoints(); // make sure it's empty
	LandscapeSplineComp->CopyToSplineComponent(PathSplineComponent);
	PathSplineComponent->UpdateSpline(); // recalculate spline data

	if (PathSplineComponent->GetNumberOfSplinePoints() < 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("ObstacleSpawnerActor: Failed to convert LandscapeSplineActor to SplineComponent."));
		return false;
	}

	return true;
}

FVector AObstacleSpawnerActor::GetRandomPointAlongSpline(const float SplineLength)
{
	float RandomDistance = FMath::FRandRange(0.0f, SplineLength);
	FTransform SplineTransform = PathSplineComponent->GetTransformAtDistanceAlongSpline(RandomDistance, ESplineCoordinateSpace::World);
	FVector SplineLocation = SplineTransform.GetLocation();
	FVector SplineRightVector = SplineTransform.GetRotation().GetRightVector();

	float RandomOffset = FMath::FRandRange(MinOffsetDistance, MaxOffsetDistance);
	float Direction = FMath::RandBool() ? 1.0f : -1.0f; // left or right
	FVector Offset = SplineRightVector * RandomOffset * Direction;
	return SplineLocation + Offset;
}

AActor* AObstacleSpawnerActor::CreateObstacle(const FVector &SpawnLocation)
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();

	AActor* NewObstacle = World->SpawnActor<AActor>(AActor::StaticClass(), SpawnLocation, FRotator::ZeroRotator, SpawnParams);

	if (NewObstacle)
	{
		UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(NewObstacle, TEXT("ObstacleMeshComponent"));
		if (MeshComp)
		{
			MeshComp->RegisterComponent();
			NewObstacle->SetRootComponent(MeshComp);

			MeshComp->SetStaticMesh(ObstacleMesh);
			MeshComp->SetWorldLocation(SpawnLocation);

			// set collision and physics
			MeshComp->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
			MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			MeshComp->SetSimulatePhysics(true);

			SpawnedObstaclesLocations.Add(SpawnLocation);
		}
		else
		{
			NewObstacle->Destroy();
		}
	}
	return NewObstacle;
}