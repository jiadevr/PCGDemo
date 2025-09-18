// Fill out your copyright notice in the Description page of Project Settings.


#include "Road/IntersectionMeshGenerator.h"

#include "NotifyUtilities.h"
#include "Components/DynamicMeshComponent.h"
#include "Road/RoadSegmentStruct.h"
#include "Components/SplineComponent.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "Kismet/KismetMathLibrary.h"
#include "Road/RoadGeometryUtilities.h"

static TAutoConsoleVariable<bool> CVarOnlyDebugPoint(
	TEXT("RIG.OnlyDebugPoint"), false,TEXT("Only Generate Points Ignore Meshes"), ECVF_Default);
static TAutoConsoleVariable<bool> CVarHideGraphicDebug(
	TEXT("RIG.HideGraphicDebug"), false,TEXT("Only Generate Points Ignore Meshes"), ECVF_Default);

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
		return false;
	}
	TArray<FVector2D> ExtrudeShape = CreateExtrudeShape();
	if (ExtrudeShape.IsEmpty())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner(
			FString::Printf(
				TEXT("[ERROR]%s Create Intersection Failed,Spline Data Is Empty"), *Owner->GetActorLabel()));
		return false;
	}
	if (CVarOnlyDebugPoint.GetValueOnGameThread())
	{
		return true;
	}
	FGeometryScriptPrimitiveOptions GeometryScriptOptions;
	FTransform ExtrudeMeshTrans = FTransform::Identity;
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSimpleExtrudePolygon(
		MeshComponent->GetDynamicMesh(), GeometryScriptOptions, ExtrudeMeshTrans, ExtrudeShape, 30.0f);
	UGeometryScriptLibrary_MeshNormalsFunctions::AutoRepairNormals(MeshComponent->GetDynamicMesh());
	FGeometryScriptSplitNormalsOptions SplitOptions;
	FGeometryScriptCalculateNormalsOptions CalculateOptions;
	UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(MeshComponent->GetDynamicMesh(), SplitOptions,
	                                                                 CalculateOptions);
	UNotifyUtilities::ShowPopupMsgAtCorner(
		FString::Printf(TEXT("%s Generate Intersection Finished!"), *Owner->GetActorLabel()));
	return true;
}

void UIntersectionMeshGenerator::SetMeshComponent(class UDynamicMeshComponent* InMeshComponent)
{
	if (InMeshComponent != nullptr)
	{
		MeshComponent = TWeakObjectPtr<UDynamicMeshComponent>(InMeshComponent);
	}
}

/*int32 UIntersectionMeshGenerator::GetOverlapSegmentOnGivenSpline(TWeakObjectPtr<USplineComponent> TargetSpline)
{
	for (auto SingleIntersectionData : IntersectionsData)
	{
		if (SingleIntersectionData.OwnerSpline == TargetSpline)
		{
			return -1;
		}
	}
	return 0;
}*/

TArray<FVector2D> UIntersectionMeshGenerator::CreateExtrudeShape()
{
	OccupiedBox.Init();
	bool bShowDebug = !CVarHideGraphicDebug.GetValueOnGameThread();
	//保持整体连贯性，所有FVector转成2D计算
	TArray<FVector2D> IntersectionConstructionPoints;
	AActor* Owner = GetOwner();
	if (nullptr == GetOwner())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Error:Found Null Owner");
		return IntersectionConstructionPoints;
	}
	const int32 IntersectionSegmentNum = IntersectionsData.Num();
	IntersectionConstructionPoints.Reserve(IntersectionSegmentNum * 10);
	const FVector2D CenterLocation(Owner->GetActorLocation());
	//每个中心线生成左右两个线段，共4个端点
	TArray<FVector2D> RoadEdgePoints;
	RoadEdgePoints.Reserve(IntersectionSegmentNum * 4);
	//为了让Segment可以相交，需要把线段延长
	static const double SegmentScalar = 2.0;
	//先计算Offset之后的点，先右后左
	for (int32 i = 0; i < IntersectionSegmentNum; i++)
	{
		const FVector2D CurrentSegmentEndPoint2D(IntersectionsData[i].IntersectionEndPointWS);
		if (bShowDebug)
		{
			//中心点显示流入为绿色，流出为橙色
			DrawDebugSphere(GetWorld(), IntersectionsData[i].IntersectionEndPointWS, 20.0f, 8,
			                (IntersectionsData[i].bIsFlowIn ? FColor::Green : FColor::Orange), true, -1, 0,
			                5);
		}
		const FVector2D VectorToCenter = FVector2D(CenterLocation - CurrentSegmentEndPoint2D);
		//const FVector2D FlowDir =(VectorToCenter /* (IntersectionsData[i].bIsFlowIn ? 1.0 : -1.0)*/).GetSafeNormal();

		FVector2D RightEdge = VectorToCenter.GetSafeNormal().GetRotated(90.0) * IntersectionsData[i].
			RoadWidth * 0.5;
		//以FlowDir为基准,右侧两个点
		FVector2D RightMid = CurrentSegmentEndPoint2D + RightEdge;
		FVector2D RightStart = RightMid - VectorToCenter * SegmentScalar;
		RoadEdgePoints.Emplace(RightStart);
		FVector2D RightEnd = RightMid + VectorToCenter * SegmentScalar;
		RoadEdgePoints.Emplace(RightEnd);
		if (bShowDebug)
		{
			//右侧红线
			DrawDebugDirectionalArrow(GetWorld(), FVector(RightStart, 0.0), FVector(RightEnd, 0.0), 100.0f, FColor::Red,
			                          true);
		}

		//以FlowDir为基准,左侧两个点
		FVector2D LeftMid = CurrentSegmentEndPoint2D + RightEdge * -1.0;
		FVector2D LeftStart = LeftMid - VectorToCenter * SegmentScalar;
		RoadEdgePoints.Emplace(LeftStart);
		FVector2D LeftEnd = LeftMid + VectorToCenter * SegmentScalar;
		RoadEdgePoints.Emplace(LeftEnd);
		if (bShowDebug)
		{
			//左侧蓝线
			DrawDebugDirectionalArrow(GetWorld(), FVector(LeftStart, 0.0), FVector(LeftEnd, 0.0), 100.0f, FColor::Blue,
			                          true);
		}
	}
	//由于传入节点已经排序，线段只会和相邻的相交，单循环可以解决
	//记录样条相交情况和交点Index，交点保存于EdgeIntersections
	TMap<TPair<int32, int32>, int32> Visited;
	TArray<FVector2D> EdgeIntersections;
	EdgeIntersections.Reserve(IntersectionSegmentNum);
	for (int32 i = 0; i < IntersectionSegmentNum; i++)
	{
		for (int32 j = -1; j <= 1; j += 2)
		{
			//线段经过逆时针排序，右侧线段为i-1，左侧线段为i+1；函数不能接受负值
			int32 TargetSegmentIndex = FMath::Modulo(i + j + IntersectionSegmentNum, IntersectionSegmentNum);
			//统一格式方便后续剪枝
			TPair<int32, int32> Visitor;
			if (IntersectionSegmentNum > 2)
			{
				Visitor.Key = i < TargetSegmentIndex ? i : TargetSegmentIndex;
				Visitor.Value = i < TargetSegmentIndex ? TargetSegmentIndex : i;
				if (Visitor.Key == 0 && Visitor.Value == IntersectionSegmentNum - 1)
				{
					Visitor.Key = Visitor.Value;
					Visitor.Value = 0;
				}
			}
			//这种情况是仅有两条线相交的时候能创建对侧
			else
			{
				Visitor.Key = TargetSegmentIndex;
				Visitor.Value = i;
			}
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
			FVector2D CurrentSegmentEdgeStart = RoadEdgePoints[4 * i + 1 + j];
			FVector2D CurrentSegmentEdgeEnd = RoadEdgePoints[4 * i + 2 + j];
			FVector2D TargetSegmentEdgeStart = RoadEdgePoints[4 * TargetSegmentIndex + 1 + (-j)];
			FVector2D TargetSegmentEdgeEnd = RoadEdgePoints[4 * TargetSegmentIndex + 2 + (-j)];
			FVector2D EdgeIntersectionWS;
			int ResultIndex = -1;
			if (URoadGeometryUtilities::Get2DIntersection(CurrentSegmentEdgeStart, CurrentSegmentEdgeEnd,
			                                              TargetSegmentEdgeStart, TargetSegmentEdgeEnd,
			                                              EdgeIntersectionWS))
			{
				ResultIndex = EdgeIntersections.Emplace(EdgeIntersectionWS);
				if (bShowDebug)
				{
					DrawDebugSphere(GetWorld(), FVector(EdgeIntersectionWS, 0.0), 20.0f, 8,
					                FColor::Cyan, true, -1, 0,
					                5);
				}
			}
			Visited.Emplace(Visitor, ResultIndex);
		}
	}
	//交点计算完毕获得在相交内容之间插值获得弧线过渡
	//不添加SplineComponent，直接使用
	FInterpCurveVector2D TransitionalSpline;
	TArray<FVector2D> TransitionalSplinePoints;
	TransitionalSplinePoints.SetNum(10);
	//Set不保序，所以后边依然需要排序
	for (const auto& EdgeIntersectionElem : Visited)
	{
		if (-1 == EdgeIntersectionElem.Value)
		{
			//ensureMsgf(false, TEXT("Find Null Intersection Between Neighbors"));
			continue;
		}
		FVector2D EdgeIntersectionLoc = EdgeIntersections[EdgeIntersectionElem.Value];

		//这里拿到的是样条编号，小的排在前边，也就是说每一段都从左到右，取Start的左边界和To的右边界
		int32 FromSegmentIndex = EdgeIntersectionElem.Key.Key;
		FVector2D FromEdgeStartLoc = RoadEdgePoints[4 * FromSegmentIndex + 2];
		FVector2D FromEdgeTangent = CalTransitionalTangentOnEdge(EdgeIntersectionLoc, FromEdgeStartLoc);
		FInterpCurvePoint FromPoint(0.0, FromEdgeStartLoc, -FromEdgeTangent, FromEdgeTangent, CIM_CurveAuto);
		OccupiedBox += FromEdgeStartLoc;
		int32 ToSegmentIndex = EdgeIntersectionElem.Key.Value;
		FVector2D ToEdgeStartLoc = RoadEdgePoints[4 * ToSegmentIndex];
		FVector2D ToEdgeTangent = CalTransitionalTangentOnEdge(EdgeIntersectionLoc, ToEdgeStartLoc);
		FInterpCurvePoint ToPoint(1.0, ToEdgeStartLoc, -ToEdgeTangent, ToEdgeTangent, CIM_CurveAuto);
		OccupiedBox += ToEdgeStartLoc;

		TransitionalSpline.Reset();
		TransitionalSpline.Points.Add(FromPoint);
		TransitionalSpline.Points.Add(ToPoint);
		TransitionalSplinePoints[0] = FromEdgeStartLoc;
		for (int i = 1; i < 9; ++i)
		{
			float InputKey = i / 10.0f;
			FVector2D SubdivisionLoc = TransitionalSpline.Eval(InputKey, FVector2D::Zero());
			TransitionalSplinePoints[i] = SubdivisionLoc;
		}
		TransitionalSplinePoints[9] = ToEdgeStartLoc;
		IntersectionConstructionPoints.Append(TransitionalSplinePoints);
	}
	//TODO:这个排序算法需要优化每段本身基本有序;两线交点在判断同一侧时会出现点混乱
	if (Visited.Num() > 2)
	{
		URoadGeometryUtilities::SortPointCounterClockwise(CenterLocation, IntersectionConstructionPoints);
	}
	for (int i = 0; i < IntersectionConstructionPoints.Num(); ++i)
	{
		uint8 ColorGreenDepth = i * 255 / IntersectionConstructionPoints.Num();
		DrawDebugSphere(GetWorld(), FVector(IntersectionConstructionPoints[i], 0.0), 20.0f, 8,
		                FColor(0, 0, ColorGreenDepth), true, -1, 0,
		                5);
		//世界空间转局部空间
		IntersectionConstructionPoints[i] = IntersectionConstructionPoints[i] - CenterLocation;
	}

	return IntersectionConstructionPoints;
}


FVector2D UIntersectionMeshGenerator::CalTransitionalTangentOnEdge(const FVector2D& Intersection,
                                                                   const FVector2D& EdgePoint)
{
	FVector2D Dir(UKismetMathLibrary::GetDirectionUnitVector(FVector(EdgePoint, 0.0), FVector(Intersection, 0.0)));
	FVector2D Tangent = FVector2D::Distance(EdgePoint, Intersection) * 2 * Dir;
	return Tangent;
}
