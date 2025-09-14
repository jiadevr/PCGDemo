// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MeshGeneratorInterface.h"
#include "Road/RoadSegmentStruct.h"
#include "Components/ActorComponent.h"
#include "IntersectionMeshGenerator.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CITYGENERATOR_API UIntersectionMeshGenerator : public UActorComponent, public IMeshGeneratorInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UIntersectionMeshGenerator();

	//UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly)
	//FSplineIntersection IntersectionData;

	void SetIntersectionSegmentsData(const TArray<FIntersectionSegment>& InIntersectionData);


	virtual bool GenerateMesh() override;
	virtual void SetMeshComponent(class UDynamicMeshComponent* InMeshComponent) override;
protected:
	[[nodiscard]]TArray<FVector2D> CreateExtrudeShape();

	FVector CalculateTangentPoint(const FVector& Intersection, const FVector& EdgePoint);

	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly)
	TArray<FIntersectionSegment> IntersectionsData;
};
