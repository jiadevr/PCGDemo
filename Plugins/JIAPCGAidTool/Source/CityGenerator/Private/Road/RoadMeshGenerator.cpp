// Fill out your copyright notice in the Description page of Project Settings.


#include "Road/RoadMeshGenerator.h"
#include "Components/DynamicMeshComponent.h"


// Sets default values for this component's properties
URoadMeshGenerator::URoadMeshGenerator()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

void URoadMeshGenerator::DrawDebugElemOnSweepPoint()
{
	AActor* OwnerActor = GetOwner();
	if (nullptr == OwnerActor)
	{
		return;
	}
	FTransform OwnerTransform = OwnerActor->GetActorTransform();
	if (DebugIndex < 0)
	{
		for (int32 i = 0; i < SweepPointsTrans.Num(); ++i)
		{
			DrawDebugPoint(
				GetWorld(), OwnerTransform.TransformPosition(SweepPointsTrans[i].GetLocation()) + FVector(0, 0, 50.0f),
				5.0f,
				FColor::Purple, false, 10.0f);
			DrawDebugString(
				GetWorld(), OwnerTransform.TransformPosition(SweepPointsTrans[i].GetLocation()) + FVector(0, 0, 60.0f),
				FString::FromInt(i),
				nullptr, FColor::Purple, 10.0f, false, 5.0f);
		}
		return;
	}
	DrawDebugPoint(GetWorld(), OwnerTransform.TransformPosition(SweepPointsTrans[DebugIndex].GetLocation()), 5.0f,
	               FColor::Purple, false, 10.0f);
}

bool URoadMeshGenerator::GenerateMesh()
{
	return false;
}

void URoadMeshGenerator::SetMeshComponent(class UDynamicMeshComponent* InMeshComponent)
{
	if (InMeshComponent != nullptr)
	{
		MeshComponent = TWeakObjectPtr<UDynamicMeshComponent>(InMeshComponent);
	}
}
