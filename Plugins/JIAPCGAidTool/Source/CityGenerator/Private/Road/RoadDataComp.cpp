// Fill out your copyright notice in the Description page of Project Settings.


#include "Road/RoadDataComp.h"


// Sets default values for this component's properties
URoadDataComp::URoadDataComp()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

void URoadDataComp::DrawDebugElemOnSweepPoint()
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
			DrawDebugPoint(GetWorld(), OwnerTransform.TransformPosition(SweepPointsTrans[i].GetLocation()) + FVector(0, 0, 50.0f), 5.0f,
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


// Called when the game starts
void URoadDataComp::BeginPlay()
{
	Super::BeginPlay();

	// ...
}


// Called every frame
void URoadDataComp::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}
