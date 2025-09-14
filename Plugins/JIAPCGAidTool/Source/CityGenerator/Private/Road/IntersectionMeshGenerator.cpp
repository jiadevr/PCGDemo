// Fill out your copyright notice in the Description page of Project Settings.


#include "Road/IntersectionMeshGenerator.h"

#include "NotifyUtilities.h"
#include "Components/DynamicMeshComponent.h"
#include "Road/RoadSegmentStruct.h"
#include "Components/SplineComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Road/RoadGeometryUtilities.h"


// Sets default values for this component's properties
UIntersectionMeshGenerator::UIntersectionMeshGenerator()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
}

void UIntersectionMeshGenerator::SetIntersectionSegmentsData(const TArray<FIntersectionSegment>& InIntersectionData)
{
	IntersectionsData = InIntersectionData;
}

bool UIntersectionMeshGenerator::GenerateMesh()
{
	AActor* Owner = GetOwner();
	ensureAlwaysMsgf(nullptr!=Owner, TEXT("Component Has No Owner"));
	if (!MeshComponent.IsValid())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner(
			FString::Printf(
				TEXT("[ERROR]%s Create Intersection Failed,Null MeshComp Found!"), *Owner->GetActorLabel()));
		return false;
	}
	if (IntersectionsData.IsEmpty())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner(
			FString::Printf(
				TEXT("[ERROR]%s Create Intersection Failed,Spline Data Is Empty"), *Owner->GetActorLabel()));
	}
	TArray<FVector2D> ExtrudeShape = CreateExtrudeShape();
	//@TODO:Extrue路口
	return false;
}

void UIntersectionMeshGenerator::SetMeshComponent(class UDynamicMeshComponent* InMeshComponent)
{
	if (InMeshComponent != nullptr)
	{
		MeshComponent = TWeakObjectPtr<UDynamicMeshComponent>(InMeshComponent);
	}
}

TArray<FVector2D> UIntersectionMeshGenerator::CreateExtrudeShape()
{
	//
	TArray<FVector2D> IntersectionSegments;
	AActor* Owner = GetOwner();
	if (nullptr == GetOwner())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Error:Found Null Owner");
		return IntersectionSegments;
	}

	const int32 IntersectionSegmentNum = IntersectionsData.Num();
	const FVector CenterLocation = Owner->GetActorLocation();
	//每个中心线生成左右两个线段，共4个端点
	TArray<FVector> RoadEdgePoints;
	RoadEdgePoints.Reserve(IntersectionSegmentNum * 4);
	//为了让Segment可以相交，需要把线段延长
	static const double SegmentScalar = 1.25;
	//先计算Offset之后的点，先右后左
	for (int32 i = 0; i < IntersectionSegmentNum; i++)
	{
		FVector VectorToCenter = IntersectionsData[i].IntersectionEndPointWS - CenterLocation;
		FVector FlowDir = (VectorToCenter * (IntersectionsData[i].bIsFlowIn ? -1.0 : 1.0)).GetSafeNormal();
		FVector RightEdge = FVector::CrossProduct(FlowDir, FVector::UpVector) * IntersectionsData[i].
			RoadWidth * 0.5;
		//以FlowDir为基准,右侧两个点
		RoadEdgePoints.Emplace(IntersectionsData[i].IntersectionEndPointWS + RightEdge);
		RoadEdgePoints.
			Emplace(IntersectionsData[i].IntersectionEndPointWS + RightEdge + VectorToCenter * SegmentScalar);
		//以FlowDir为基准,左侧两个点
		RoadEdgePoints.Emplace(IntersectionsData[i].IntersectionEndPointWS + (RightEdge * -1.0));
		RoadEdgePoints.
			Emplace(IntersectionsData[i].IntersectionEndPointWS + (RightEdge * -1.0) + VectorToCenter * SegmentScalar);
	}
	//由于传入节点已经排序，线段只会和相邻的相交，单循环可以解决
	//记录样条相交情况和交点Index，交点保存于EdgeIntersections
	TMap<TPair<int32, int32>, int32> Visited;
	TArray<FVector> EdgeIntersections;
	EdgeIntersections.Reserve(IntersectionSegmentNum);
	for (int32 i = 0; i < IntersectionSegmentNum; i++)
	{
		for (int32 j = -1; j <= 1; j += 2)
		{
			//线段经过逆时针排序，右侧线段为i-1，左侧线段为i+1
			int32 TargetSegmentIndex = FMath::Modulo(i + j, IntersectionSegmentNum);
			TPair<int32, int32> Visitor;
			Visitor.Key = i < TargetSegmentIndex ? i : j;
			Visitor.Value = i < TargetSegmentIndex ? j : i;
			//已经访问过
			if (Visited.Contains(Visitor))
			{
				continue;
			}
			//右侧Segment的左边和当前样条右边求交，左侧Segment的右边和当前样条左边求交
			//右边ID为i*4;右边ID为I*4+2
			/* 计算和右侧Segment（目标）交点时需要计算本段Segment右侧边线和目标Segment左侧边线的交点
			 * 本段Segment右侧边线对应Index 4*i; 4*i+1
			 * 目标Segment左侧边线对应Index 4*(i-1)+2; 4*(i-1)+3
			 * 计算和左侧Segment（目标）交点时需要计算本段Segment左侧边线和目标Segment右侧边线的交点
			 * 本段Segment左侧边线对应Index 4*i+2; 4*i+3
			 * 目标Segment左侧边线对应Index 4*(i-1)+0; 4*(i-1)+1
			 */
			FVector2D CurrentSegmentEdgeStart = FVector2D(RoadEdgePoints[4 * i + 1 + j]);
			FVector2D CurrentSegmentEdgeEnd = FVector2D(RoadEdgePoints[4 * i + 2 + j]);
			FVector2D TargetSegmentEdgeStart = FVector2D(RoadEdgePoints[4 * TargetSegmentIndex + 1 + (-j)]);
			FVector2D TargetSegmentEdgeEnd = FVector2D(RoadEdgePoints[4 * TargetSegmentIndex + 2 + (-j)]);
			FVector2D EdgeIntersectionWS;
			int ResultIndex = -1;
			if (URoadGeometryUtilities::Get2DIntersection(CurrentSegmentEdgeStart, CurrentSegmentEdgeEnd,
			                                              TargetSegmentEdgeStart, TargetSegmentEdgeEnd,
			                                              EdgeIntersectionWS))
			{
				ResultIndex = EdgeIntersections.Emplace(FVector(EdgeIntersectionWS, 0.0));
			}
			Visited.Emplace(Visitor, ResultIndex);
		}
	}
	//交点计算完毕获得在相交内容之间插值获得弧线过渡
	for (const auto& EdgeIntersectionElem : Visited)
	{
		int32 FromEdgeIndex = EdgeIntersectionElem.Key.Key;
		//@TODO:缺Rotator,得传进来
		FSplinePoint FromPoint(0.0, RoadEdgePoints[FromEdgeIndex], ESplinePointType::Curve);
		int32 ToEdgeIndex = EdgeIntersectionElem.Key.Value;
		FSplinePoint ToPoint(1.0, RoadEdgePoints[ToEdgeIndex], ESplinePointType::Curve);
		int32 IntersectionIndex = EdgeIntersectionElem.Value;
		FSplinePoint TransactionPoint(0.5, EdgeIntersections[IntersectionIndex], ESplinePointType::Curve);
		if (-1 == IntersectionIndex)
		{
			ensureAlwaysMsgf(false, TEXT("Find Null Intersection Between Neighbors"));
			continue;
		}
		FSpline TransactionSpline;
		//@TODO:细分过渡段，需要看SplineComponent的实现方式
		TransactionSpline.AddPoint()
	}
	//@TODO：整理点顺序
	return IntersectionSegments;
}


FVector UIntersectionMeshGenerator::CalculateTangentPoint(const FVector& Intersection, const FVector& EdgePoint)
{
	FVector Dir = UKismetMathLibrary::GetDirectionUnitVector(EdgePoint, Intersection);
	FVector Tangent = FVector::Dist(EdgePoint, Intersection) * 2 * Dir;
	return Tangent;
}
