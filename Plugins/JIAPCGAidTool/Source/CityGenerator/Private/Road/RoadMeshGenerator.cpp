// Fill out your copyright notice in the Description page of Project Settings.


#include "Road/RoadMeshGenerator.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"


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

void URoadMeshGenerator::SetRoadPathTransform(const TArray<FTransform>& InTransforms)
{
	SweepPointsTrans = InTransforms;
}

void URoadMeshGenerator::SetReferenceSpline(TWeakObjectPtr<USplineComponent> InReferenceSpline)
{
	ReferenceSpline = InReferenceSpline;
}

void URoadMeshGenerator::SetRoadType(ELaneType InRoadType)
{
	switch (InRoadType)
	{
	case ELaneType::COLLECTORROADS:
		RoadInfo = FLaneMeshInfo(500.0f);
		break;
	case ELaneType::ARTERIALROADS:
		RoadInfo = FLaneMeshInfo(1000.0f);
		break;
	case ELaneType::EXPRESSWAYS:
		RoadInfo = FLaneMeshInfo(2000.0f);
		break;
	default:
		RoadInfo = FLaneMeshInfo(500.0f);
		break;
	}
}

void URoadMeshGenerator::SetRoadInfo(const FRoadSegmentsGroup& InRoadWithConnect)
{
	int32 SweepPathLength=InRoadWithConnect.ContinuousSegmentsTrans.Num();
	SweepPathLength+=InRoadWithConnect.bHasHeadConnection?1:0;
	SweepPathLength+=InRoadWithConnect.bHasTailConnection?1:0;
	SweepPointsTrans.Reserve(SweepPathLength);
	if (InRoadWithConnect.bHasHeadConnection)
	{
		SweepPointsTrans.Emplace(InRoadWithConnect.HeadConnectionTrans);
	}
	SweepPointsTrans.Append(InRoadWithConnect.ContinuousSegmentsTrans);
	if (InRoadWithConnect.bHasTailConnection)
	{
		SweepPointsTrans.Emplace(InRoadWithConnect.TailConnectionTrans);
	}
}

bool URoadMeshGenerator::GenerateMesh()
{
	if (SweepPointsTrans.IsEmpty() && Connections.IsEmpty())
	{
		return false;
	}
	//MergeConnectionsIntoSweepPoints();
	for (int32 i = 1; i < SweepPointsTrans.Num(); ++i)
	{
		FVector CenterLocation = (SweepPointsTrans[i - 1].GetLocation() + SweepPointsTrans[i].GetLocation()) / 2.0;
		double BoxHalfLength = FVector::Dist(SweepPointsTrans[i - 1].GetLocation(), SweepPointsTrans[i].GetLocation()) /
			2.0;
		FVector BoxExtent(BoxHalfLength, 250.0, 10);
		FBox DebugBox(-BoxExtent, BoxExtent);
		FRotator BoxRotator = UKismetMathLibrary::MakeRotFromX(
			(SweepPointsTrans[i].GetLocation() - SweepPointsTrans[i - 1].GetLocation()).GetSafeNormal());
		FTransform DebugBoxTrans(BoxRotator, CenterLocation);
		DrawDebugSolidBox(this->GetWorld(), DebugBox, FColor::Cyan, DebugBoxTrans, true);
	}
	return false;
}

void URoadMeshGenerator::SetMeshComponent(class UDynamicMeshComponent* InMeshComponent)
{
	if (InMeshComponent != nullptr)
	{
		MeshComponent = TWeakObjectPtr<UDynamicMeshComponent>(InMeshComponent);
	}
}

/*void URoadMeshGenerator::MergeConnectionsIntoSweepPoints()
{
	if (Connections.IsEmpty() || !ReferenceSpline.IsValid() || nullptr == this->GetOwner())
	{
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("[MergePath] %s Revecive %d Connections"), *this->GetOwner()->GetActorLabel(),
	       Connections.Num());
	TArray<FTransform> ConnectPointsTrans;
	for (const FIntersectionSegment& Connection : Connections)
	{
		const FVector ConnectPointLoc = Connection.IntersectionEndPointWS;
		USplineComponent* PathSpline = ReferenceSpline.Pin().Get();
		float SplineDistance = PathSpline->GetDistanceAlongSplineAtLocation(
			ConnectPointLoc, ESplineCoordinateSpace::World);
		FTransform ConnectTrans = PathSpline->GetTransformAtDistanceAlongSpline(
			SplineDistance, ESplineCoordinateSpace::World);
		ConnectPointsTrans.Emplace(ConnectTrans);
	}
	if (!SweepPointsTrans.IsEmpty())
	{
		FVector StartLocation = SweepPointsTrans[0].GetLocation();
		FVector EndLocation = SweepPointsTrans.Last(0).GetLocation();
		for (const auto& ConnectionTrans : ConnectPointsTrans)
		{
			float DistanceToStart = FVector::Dist2D(ConnectionTrans.GetLocation(), StartLocation);
			float DistanceToEnd = FVector::Dist2D(ConnectionTrans.GetLocation(), EndLocation);
			int32 InsertIndex = DistanceToStart < DistanceToEnd ? 0 : SweepPointsTrans.Num();
			SweepPointsTrans.Insert(ConnectionTrans, InsertIndex);
		}
	}
	else
	{
		SweepPointsTrans.Append(ConnectPointsTrans);
	}
}*/
