// Fill out your copyright notice in the Description page of Project Settings.


#include "Road/IntersectionMeshGenerator.h"
#include "Components/DynamicMeshComponent.h"


// Sets default values for this component's properties
UIntersectionMeshGenerator::UIntersectionMeshGenerator()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

bool UIntersectionMeshGenerator::GenerateMesh()
{
	return false;
}

void UIntersectionMeshGenerator::SetMeshComponent(class UDynamicMeshComponent* InMeshComponent)
{
	if (InMeshComponent != nullptr)
	{
		MeshComponent = TWeakObjectPtr<UDynamicMeshComponent>(InMeshComponent);
	}
}
