// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "ObstacleSpawnerActor.generated.h"

class ALandscapeSplineActor;
class USplineComponent;
class UStaticMesh;

UCLASS()
class DRIVERLESSTASK_API AObstacleSpawnerActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AObstacleSpawnerActor();

	// Track
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cone Spawning|Setup")
	ALandscapeSplineActor* TrackSplineActor;

	// Obstacle Mesh to spawn around
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cone Spawning|Setup")
	UStaticMesh* ObstacleMesh;

	// Number of obstacles to spawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cone Spawning|Parameters", meta = (ClampMin = "0"))
	int32 NumberOfObstacles = 50;

	// Min and max distance from spline's centerline (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cone Spawning|Parameters")
	float MinOffsetDistance = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cone Spawning|Parameters")
	float MaxOffsetDistance = 400.0f;

	// Minimum distance between placed obstacles (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cone Spawning|Parameters", meta = (ClampMin = "0"))
	float MinDistanceBetweenObstacles = 150.0f;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:
	void SpawnObstacles();
	bool CheckRequirements();
	FVector GetRandomPointAlongSpline(const float SplineLength);
	AActor* CreateObstacle(const FVector &SpawnLocation);

	// temp spline component to access spline points
	UPROPERTY()
	USplineComponent* PathSplineComponent;

	// already placed obstacles
	TArray<FVector> SpawnedObstaclesLocations;

	UWorld* World;

};
