// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MeshGeneratorInterface.h"
#include "RoadSegmentStruct.h"
#include "Components/ActorComponent.h"
#include "IntersectionMeshGenerator.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CITYGENERATOR_API UIntersectionMeshGenerator : public UActorComponent, public IMeshGeneratorInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UIntersectionMeshGenerator();
	
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly)
	FSplineIntersection IntersectionData;
	
	virtual bool GenerateMesh() override;
	virtual void SetMeshComponent(class UDynamicMeshComponent* InMeshComponent) override;
};
