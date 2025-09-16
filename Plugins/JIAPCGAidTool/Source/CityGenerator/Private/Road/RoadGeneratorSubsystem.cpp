// Fill out your copyright notice in the Description page of Project Settings.


#include "Road/RoadGeneratorSubsystem.h"

#include "CityGeneratorSubSystem.h"
#include "EditorComponentUtilities.h"
#include "NotifyUtilities.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SplineComponent.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "Kismet/KismetMathLibrary.h"
#include "Road/IntersectionMeshGenerator.h"
#include "Road/RoadGeometryUtilities.h"
#include "Road/RoadMeshGenerator.h"
#include "Road/RoadSegmentStruct.h"


void URoadGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	//根据黑客帝国的数据
	RoadPresetMap.Emplace(ELaneType::COLLECTORROADS, FLaneMeshInfo(500.0f, 20.0f));
	RoadPresetMap.Emplace(ELaneType::ARTERIALROADS, FLaneMeshInfo(1000.0f, 20.0f));
	RoadPresetMap.Emplace(ELaneType::EXPRESSWAYS, FLaneMeshInfo(2000.0f, 20.0f));
	ComponentMoveHandle = GEditor->OnComponentTransformChanged().AddUObject(
		this, &URoadGeneratorSubsystem::OnLevelComponentMoved);
}


void URoadGeneratorSubsystem::OnLevelComponentMoved(USceneComponent* MovedComp, ETeleportType MoveType)
{
	if (nullptr == MovedComp)
	{
		return;
	}
	USplineComponent* SplineComp = Cast<USplineComponent>(MovedComp);
	if (nullptr == SplineComp)
	{
		return;
	}
	TWeakObjectPtr<USplineComponent> PotentialSpline(SplineComp);
	if (SplineSegmentsInfo.Contains(PotentialSpline))
	{
		bNeedRefreshSegmentData = true;
	}
}

void URoadGeneratorSubsystem::Deinitialize()
{
	SplineSegmentsInfo.Empty();
	SplineQuadTree.Empty();
	RoadIntersectionsComps.Empty();
	GEditor->OnComponentTransformChanged().Remove(ComponentMoveHandle);
	Super::Deinitialize();
}
#pragma region GenerateIntersection
void URoadGeneratorSubsystem::GenerateIntersections()
{
	//更新样条信息
	if (bNeedRefreshSegmentData)
	{
		InitialRoadSplines();
	}
	if (SplineSegmentsInfo.IsEmpty())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Error:Find Null Spline");
		return;
	}
	//计算样条交点
	TArray<FSplineIntersection> IntersectionResults = FindAllIntersections();
	if (IntersectionResults.IsEmpty())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Error:Find Null Intersections");
		return;
	}
	RoadIntersectionsComps.Reserve(IntersectionResults.Num());
	RoadIntersectionsComps.Reset();

	TArray<FIntersectionSegment> IntersectionBuildData;
	//对每一个交点生成Actor挂载
	for (int32 i = 0; i < IntersectionResults.Num(); ++i)
	{
		//切割交点分段
		if (TearIntersectionToSegments(IntersectionResults[i], IntersectionBuildData))
		{
			//生成交点对象
			FTransform ActorTransform = FTransform::Identity;
			ActorTransform.SetLocation(IntersectionResults[i].WorldLocation);
			AActor* IntersectionActor = UEditorComponentUtilities::SpawnEmptyActor(
				FString::Printf(TEXT("RoadIntersection%d"), i), ActorTransform);
			ensureAlwaysMsgf(IntersectionActor!=nullptr, TEXT("Error:Create Intersection Actor Failed"));

			UActorComponent* MeshCompTemp = UEditorComponentUtilities::AddComponentInEditor(
				IntersectionActor, UDynamicMeshComponent::StaticClass());
			UDynamicMeshComponent* MeshComp = Cast<UDynamicMeshComponent>(MeshCompTemp);
			ensureAlwaysMsgf(MeshComp!=nullptr, TEXT("Error:Create DynamicMeshComp Failed"));
			UActorComponent* GeneratorCompTemp = UEditorComponentUtilities::AddComponentInEditor(
				IntersectionActor, UIntersectionMeshGenerator::StaticClass());
			UIntersectionMeshGenerator* GeneratorComp = Cast<UIntersectionMeshGenerator>(GeneratorCompTemp);
			ensureAlwaysMsgf(GeneratorComp!=nullptr, TEXT("Error:Create IntersectionMeshGeneratorComp Failed"));
			GeneratorComp->SetMeshComponent(MeshComp);
			GeneratorComp->SetIntersectionSegmentsData(IntersectionBuildData);
			RoadIntersectionsComps.Emplace(GeneratorComp);
		}
	}

	FlushPersistentDebugLines(GetWorld());
	//调用生成
	for (const auto& IntersectionGenerator : RoadIntersectionsComps)
	{
		IntersectionGenerator->GenerateMesh();
	}
}

bool URoadGeneratorSubsystem::InitialRoadSplines()
{
	UCityGeneratorSubSystem* DataSubsystem = GEditor->GetEditorSubsystem<UCityGeneratorSubSystem>();
	if (!DataSubsystem)
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Error::Find Null UCityGeneratorSubSystem");
		return false;
	}
	TSet<TWeakObjectPtr<USplineComponent>> RoadSplines = DataSubsystem->GetSplines();
	if (RoadSplines.IsEmpty())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Error::Find Null Spline");
		return false;
	}

	for (TWeakObjectPtr<USplineComponent> SplineComponent : RoadSplines)
	{
		USplineComponent* PinnedSplineComp = SplineComponent.Pin().Get();
		UpdateSplineSegments(PinnedSplineComp);
	}

	return true;
}

void URoadGeneratorSubsystem::UpdateSplineSegments(USplineComponent* TargetSpline, float SampleDistance)
{
	if (nullptr == TargetSpline)
	{
		return;
	}
	TArray<FVector> PolyLinePoints;
	TargetSpline->ConvertSplineToPolyLine(ESplineCoordinateSpace::World, SampleDistance * SampleDistance,
	                                      PolyLinePoints);
	if (PolyLinePoints.IsEmpty())
	{
		ensureAlwaysMsgf(!PolyLinePoints.IsEmpty(),TEXT("Get Empty PolyPointArray"));
		return;
	}
	TArray<FSplinePolyLineSegment> Segments;
	const int32 SegmentCount = PolyLinePoints.Num() - 1;
	Segments.SetNum(SegmentCount);
	FTransform PreviousTrans;
	for (int32 i = 0; i < PolyLinePoints.Num(); i++)
	{
		float DisOfPoint = TargetSpline->GetDistanceAlongSplineAtLocation(
			PolyLinePoints[i], ESplineCoordinateSpace::World);
		FTransform CurrentTrans = TargetSpline->GetTransformAtDistanceAlongSpline(
			DisOfPoint, ESplineCoordinateSpace::World, true);
		if (i == 0)
		{
			PreviousTrans = CurrentTrans;
			continue;
		}
		Segments[i - 1] = FSplinePolyLineSegment(TargetSpline, i - 1, SegmentCount - 1, PreviousTrans, CurrentTrans);
		PreviousTrans = CurrentTrans;
	}
	SplineSegmentsInfo.Emplace(TargetSpline, Segments);
}

TArray<FSplineIntersection> URoadGeneratorSubsystem::FindAllIntersections()
{
	TRACE_BOOKMARK(TEXT("Begin Find Intersections"));
	TArray<FSplineIntersection> Results;
	if (SplineSegmentsInfo.IsEmpty())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Find Intersection Failed, Reason:Find Null Spline");
		return Results;
	}
	//收集当前所有分段信息,额外存一份ID
	//TArray<FSplinePolyLineSegment> 
	TArray<FSplinePolyLineSegment> AllSegments;
	//构建四叉树
	//计算世界包围盒，使用ForceInit可以在后续追加包围盒时自动计算总包围盒大小,注意还有一个枚举值EForceInit容易混淆

	AllSegments.Reserve(SplineSegmentsInfo.Num() * 2);
	FBox2D TotalBounds(ForceInit);
	for (const auto& SegmentsOfSingleSpline : SplineSegmentsInfo)
	{
		if (!SegmentsOfSingleSpline.Key.IsValid())
		{
			continue;
		}
		TArray<FSplinePolyLineSegment> Segments = SegmentsOfSingleSpline.Value;
		for (const FSplinePolyLineSegment& Segment : Segments)
		{
			TotalBounds += FVector2D(Segment.StartTransform.GetLocation());
			TotalBounds += FVector2D(Segment.EndTransform.GetLocation());
			AllSegments.Emplace(Segment);
		}
	}

	SplineQuadTree = TQuadTree<FSplinePolyLineSegment>(TotalBounds, MinimumQuadSize);
	for (const auto& SegmentWithIndex : AllSegments)
	{
		FBox2D SegmentBounds(ForceInit);
		SegmentBounds += FVector2D(SegmentWithIndex.StartTransform.GetLocation());
		SegmentBounds += FVector2D(SegmentWithIndex.EndTransform.GetLocation());
		SplineQuadTree.Insert(SegmentWithIndex, SegmentBounds);
	}

	UE_LOG(LogTemp, Display, TEXT("Finish Insert To QuadTree"));
	//用于接收四叉树查询结果
	TArray<FSplinePolyLineSegment> OverlappedSegments;
	//用于缓存四叉树处理过的样条分段,使用Segment的GlobalIndex
	TSet<TPair<uint32, uint32>> ProcessedPairs;
	for (int i = 0; i < AllSegments.Num(); ++i)
	{
		FBox2D SegmentQueryBounds;
		SegmentQueryBounds += FVector2D(AllSegments[i].StartTransform.GetLocation());
		SegmentQueryBounds += FVector2D(AllSegments[i].EndTransform.GetLocation());
		//扩大范围
		SegmentQueryBounds = SegmentQueryBounds.ExpandBy(10.0f);
		//Reset不缩小内存
		OverlappedSegments.Reset();
		SplineQuadTree.GetElements(SegmentQueryBounds, OverlappedSegments);
		for (const FSplinePolyLineSegment& OverlappedSegment : OverlappedSegments)
		{
			//排除自己
			if (AllSegments[i].GetGlobalIndex() == OverlappedSegment.GetGlobalIndex())
			{
				continue;
			}
			//排除相连的同一样条的Segment
			if (AllSegments[i].OwnerSpline == OverlappedSegment.OwnerSpline)
			{
				const int32 IndexGap = FMath::Abs(AllSegments[i].SegmentIndex - OverlappedSegment.SegmentIndex);
				if (IndexGap <= 1 || IndexGap == AllSegments[i].LastSegmentIndex)
				{
					continue;
				}
			}
			//记录已经处理过的样条
			TPair<uint32, uint32> IndexPair;
			if (AllSegments[i].GetGlobalIndex() < OverlappedSegment.GetGlobalIndex())
			{
				IndexPair.Key = AllSegments[i].GetGlobalIndex();
				IndexPair.Value = OverlappedSegment.GetGlobalIndex();
			}
			else
			{
				IndexPair.Key = OverlappedSegment.GetGlobalIndex();
				IndexPair.Value = AllSegments[i].GetGlobalIndex();
			}
			//已经处理过跳过
			if (ProcessedPairs.Contains(IndexPair))
			{
				continue;
			}
			ProcessedPairs.Emplace(IndexPair);
			FVector2D IteratorSegmentStart = FVector2D(AllSegments[i].StartTransform.GetLocation());
			FVector2D IteratorSegmentEnd = FVector2D(AllSegments[i].EndTransform.GetLocation());
			FVector2D TestingSegmentStart = FVector2D(OverlappedSegment.StartTransform.GetLocation());
			FVector2D TestingSegmentEnd = FVector2D(OverlappedSegment.EndTransform.GetLocation());
			FVector2D IntersectionLoc2D;
			if (!URoadGeometryUtilities::Get2DIntersection(IteratorSegmentStart, IteratorSegmentEnd,
			                                               TestingSegmentStart, TestingSegmentEnd,
			                                               IntersectionLoc2D))
			{
				continue;
			}
			FVector FlattedIntersectionLoc = FVector(IntersectionLoc2D, 0.0);
			//检查相近点
			bool bCanMerge = false;
			for (FSplineIntersection& Result : Results)
			{
				//相当于用空间关系进行交点索引
				//@TODO:这里可能需要一个优先级算法确定以谁为终点
				if (FVector::DistSquared2D(Result.WorldLocation, FlattedIntersectionLoc) < MergeThreshold *
					MergeThreshold)
				{
					//AddSplineToOldIntersectionData
					Result.IntersectedSplines.Emplace(OverlappedSegment.OwnerSpline);
					Result.IntersectedSegmentIndex.Emplace(OverlappedSegment.SegmentIndex);
					bCanMerge = true;
					break;
				}
			}
			if (!bCanMerge)
			{
				//首次添加需要加两个Segment信息
				TArray<TWeakObjectPtr<USplineComponent>> IntersectedSplines{
					AllSegments[i].OwnerSpline, OverlappedSegment.OwnerSpline
				};
				TArray<int32> IntersectedSegmentIndex{AllSegments[i].SegmentIndex, OverlappedSegment.SegmentIndex};
				FSplineIntersection
					NewIntersection(IntersectedSplines, IntersectedSegmentIndex, FlattedIntersectionLoc);
				//AddSplineToNewIntersectionData
				Results.Emplace(NewIntersection);
			}
		}
	}
	return Results;
}

bool URoadGeneratorSubsystem::TearIntersectionToSegments(
	const FSplineIntersection& InIntersectionInfo, TArray<FIntersectionSegment>& OutSegments, float UniformDistance)
{
	if (InIntersectionInfo.IntersectedSplines.IsEmpty())
	{
		return false;
	}
	TArray<TWeakObjectPtr<USplineComponent>> IntersectedSplines = InIntersectionInfo.IntersectedSplines;
	OutSegments.Reserve(IntersectedSplines.Num());
	OutSegments.Reset();

	for (int i = 0; i < IntersectedSplines.Num(); ++i)
	{
		if (!IntersectedSplines[i].IsValid())
		{
			return false;
		}
		USplineComponent* TargetSpline = IntersectedSplines[i].Pin().Get();
		float Distance = TargetSpline->GetDistanceAlongSplineAtLocation(InIntersectionInfo.WorldLocation,
		                                                                ESplineCoordinateSpace::World);
		//判断后边一段是不是在样条上
		const float DistanceOfNextPoint = Distance + UniformDistance;
		if (DistanceOfNextPoint < TargetSpline->GetSplineLength())
		{
			FVector FlowOutPointLoc = TargetSpline->GetLocationAtDistanceAlongSpline(
				DistanceOfNextPoint, ESplineCoordinateSpace::World);
			OutSegments.Emplace(FIntersectionSegment(IntersectedSplines[i], FlowOutPointLoc, false, 500.0f));
		}
		//判断前边一段是不是在样条上
		if (Distance > UniformDistance)
		{
			FVector FlowInPointLoc = TargetSpline->GetLocationAtDistanceAlongSpline(
				Distance - UniformDistance, ESplineCoordinateSpace::World);
			OutSegments.Emplace(FIntersectionSegment(IntersectedSplines[i], FlowInPointLoc, true, 500.0f));
		}
	}
	if (OutSegments.IsEmpty())
	{
		return false;
	}
	//根据逆时针顺序排序
	FVector IntersectionPoint = InIntersectionInfo.WorldLocation;
	OutSegments.Sort([&IntersectionPoint](const FIntersectionSegment& A, const FIntersectionSegment& B)
	{
		FVector ProjectedA = FVector::VectorPlaneProject((A.IntersectionEndPointWS - IntersectionPoint),
		                                                 FVector::UnitZ());
		FVector2D RelA{ProjectedA.X, ProjectedA.Y};
		FVector ProjectedB = FVector::VectorPlaneProject((B.IntersectionEndPointWS - IntersectionPoint),
		                                                 FVector::UnitZ());
		FVector2D RelB{ProjectedB.X, ProjectedB.Y};

		float AngleA = FMath::Atan2(RelA.Y, RelA.X);
		float AngleB = FMath::Atan2(RelB.Y, RelB.X);

		// 转换为[0, 2π)范围
		if (AngleA < 0) AngleA += 2 * PI;
		if (AngleB < 0) AngleB += 2 * PI;

		if (AngleA != AngleB)
		{
			return AngleA < AngleB; // 极角小的排在前面
		}
		else
		{
			// 角度相同，按距离排序（近的在前）
			return RelA.SizeSquared() < RelB.SizeSquared();
		}
	});
	return true;
}

#pragma endregion GenerateIntersection

#pragma region GenerateRoad


void URoadGeneratorSubsystem::GenerateSingleRoadBySweep(USplineComponent* TargetSpline,
                                                        ELaneType LaneTypeEnum, float StartShrink, float EndShrink)
{
	if (nullptr == TargetSpline || nullptr == TargetSpline->GetOwner())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Fail To Create SweepMesh Invalid Spline");
		return;
	}
	FString SplineOwnerName = TargetSpline->GetOwner()->GetActorLabel() + "_GenMesh";
	FTransform SplineOwnerTransform = TargetSpline->GetOwner()->GetTransform();
	SplineOwnerTransform.SetScale3D(FVector(1.0, 1.0, 1.0));
	TObjectPtr<AActor> MeshActor = UEditorComponentUtilities::SpawnEmptyActor(SplineOwnerName, SplineOwnerTransform);
	TObjectPtr<URoadMeshGenerator> RoadDataComp = Cast<URoadMeshGenerator>(
		UEditorComponentUtilities::AddComponentInEditor(MeshActor, URoadMeshGenerator::StaticClass()));
	if (nullptr == RoadDataComp)
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Generate DataRecorder Failed");
		return;
	}
	TObjectPtr<UDynamicMeshComponent> DynamicMeshComp = Cast<UDynamicMeshComponent>(
		UEditorComponentUtilities::AddComponentInEditor(MeshActor, UDynamicMeshComponent::StaticClass()));

	if (nullptr == DynamicMeshComp)
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Generate Mesh Failed");
		return;
	}
	UDynamicMesh* DynamicMesh = DynamicMeshComp->GetDynamicMesh();
	DynamicMesh->Reset();
	FGeometryScriptPrimitiveOptions GeometryScriptOptions;
	FTransform SweepMeshTrans = FTransform::Identity;
	int32 ControlPointsCount = TargetSpline->GetNumberOfSplinePoints();
	TArray<FTransform> SweepPath;
	ResampleSamplePoint(TargetSpline, SweepPath, RoadPresetMap[LaneTypeEnum].SampleLength,
	                    0.0f, 0.0f);
	RoadDataComp->SweepPointsTrans = SweepPath;
	RoadDataComp->ReferenceSpline = TargetSpline;
	TArray<FVector2D> SweepShape = RoadPresetMap[LaneTypeEnum].CrossSectionCoord;
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(DynamicMesh, GeometryScriptOptions,
	                                                                  SweepMeshTrans, SweepShape, SweepPath);
	UGeometryScriptLibrary_MeshNormalsFunctions::AutoRepairNormals(DynamicMesh);
	FGeometryScriptSplitNormalsOptions SplitOptions;
	FGeometryScriptCalculateNormalsOptions CalculateOptions;
	UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(DynamicMesh, SplitOptions, CalculateOptions);
}

float URoadGeneratorSubsystem::GetSplineSegmentLength(const USplineComponent* TargetSpline, int32 StartPointIndex)
{
	if (StartPointIndex <= 0 || StartPointIndex >= TargetSpline->GetNumberOfSplinePoints())
	{
		return 0;
	}
	if (StartPointIndex == TargetSpline->GetNumberOfSplinePoints() - 1)
	{
		if (TargetSpline->IsClosedLoop())
		{
			return TargetSpline->GetSplineLength() - TargetSpline->GetDistanceAlongSplineAtSplinePoint(StartPointIndex);
		}
		else
		{
			return 0;
		}
	}
	return TargetSpline->GetDistanceAlongSplineAtSplinePoint(StartPointIndex) - TargetSpline->
		GetDistanceAlongSplineAtSplinePoint(StartPointIndex - 1);
}

bool URoadGeneratorSubsystem::ResampleSamplePoint(const USplineComponent* TargetSpline,
                                                  TArray<FTransform>& OutResampledTransform,
                                                  float MaxResampleDistance,
                                                  float StartShrink, float EndShrink)
{
	TArray<FTransform> ResamplePointsOnSpline;

	if (nullptr == TargetSpline)
	{
		return false;
	}
	//处理Shrink造成的前后点影响
	const int32 OriginalControlPoints = TargetSpline->GetNumberOfSplinePoints();
	const float OriginalSplineLength = TargetSpline->GetSplineLength();
	if (OriginalControlPoints == 1)
	{
		return false;
	}
	StartShrink = FMath::Max(StartShrink, 0.0f);
	EndShrink = FMath::Max(EndShrink, 0.0f);
	//只有两个控制点
	if (OriginalControlPoints == 2)
	{
		OutResampledTransform.Empty();
		OutResampledTransform.Reserve(2);
		if (TargetSpline->GetSplinePointType(0) == ESplinePointType::Linear)
		{
			//直线
			OutResampledTransform.Emplace(
				TargetSpline->GetTransformAtDistanceAlongSpline(StartShrink, ESplineCoordinateSpace::Local, true));
			//往复直线完全重合，不重复生成
			OutResampledTransform.Emplace(
				TargetSpline->GetTransformAtDistanceAlongSpline(EndShrink, ESplineCoordinateSpace::Local, true));
		}
		else
		{
			//曲线，该函数返回闭合样条返回段
			OutResampledTransform.Append(
				GetSubdivisionOnSingleSegment(TargetSpline, StartShrink, EndShrink, MaxResampleDistance, true));
		}
	}
	//具有多控制点
	else
	{
		ResamplePointsOnSpline.Reserve(OriginalControlPoints);
		//处理第一个点
		//返回ShrinkValue之后的一个点
		FTransform FirstTransform = TargetSpline->GetTransformAtDistanceAlongSpline(
			StartShrink, ESplineCoordinateSpace::Local, true);
		ResamplePointsOnSpline.Emplace(FirstTransform);
		int32 FrontTraverseIndex = 0;
		if (StartShrink > 0.0f)
		{
			while (StartShrink > TargetSpline->GetDistanceAlongSplineAtSplinePoint(FrontTraverseIndex) &&
				FrontTraverseIndex < TargetSpline->GetNumberOfSplinePoints())
			{
				FrontTraverseIndex++;
			}
			if (FrontTraverseIndex >= TargetSpline->GetNumberOfSplinePoints())
			{
				UNotifyUtilities::ShowPopupMsgAtCorner("Shrink Value Longer Than SplineValue,InValid Spline");
				return false;
			}
			//返回开区间。起始端点在前边加入进去了，但Shrink后一个控制点没有加进去
			TArray<FTransform> StartSegment = GetSubdivisionBetweenGivenAndControlPoint(
				TargetSpline, StartShrink, FrontTraverseIndex, false,
				MaxResampleDistance, false);
			ResamplePointsOnSpline.Append(StartSegment);
		}
		//Linear闭区间首个Segment结尾点添加
		if (TargetSpline->GetSplinePointType(FrontTraverseIndex - 1) == ESplinePointType::Linear)
		{
			FTransform TransOfSecondControlPoint = TargetSpline->GetTransformAtSplinePoint(
				FrontTraverseIndex, ESplineCoordinateSpace::Local, true);
			TransOfSecondControlPoint.SetRotation(FirstTransform.GetRotation());
			ResamplePointsOnSpline.Emplace(TransOfSecondControlPoint);
		}
		//处理最后一个点;数组中间插入性能不好，这里额外存了变量
		//闭合曲线在这里返回的Index值有差别
		TArray<FTransform> EndSegment;
		int32 BackTraverseIndex = TargetSpline->IsClosedLoop() ? OriginalControlPoints : OriginalControlPoints - 1;
		if (EndShrink > 0.0f)
		{
			const float StartToShrinkEnd = OriginalSplineLength - EndShrink;
			while (StartToShrinkEnd < TargetSpline->GetDistanceAlongSplineAtSplinePoint(BackTraverseIndex) &&
				BackTraverseIndex >= 0)
			{
				BackTraverseIndex--;
			}
			if (BackTraverseIndex < FrontTraverseIndex)
			{
				UNotifyUtilities::ShowPopupMsgAtCorner("Shrink Value Longer Than SplineValue,InValid Spline");
				return false;
			}
			//返回开区间，最后一段的最后一点在下面加入
			EndSegment.Append(GetSubdivisionBetweenGivenAndControlPoint(
				TargetSpline, EndShrink, BackTraverseIndex, true,
				MaxResampleDistance, false));
		}
		FTransform LastTransform = TargetSpline->GetTransformAtDistanceAlongSpline(
			(OriginalSplineLength - EndShrink), ESplineCoordinateSpace::Local, true);
		EndSegment.Emplace(LastTransform);
		//中间分段处理
		for (int i = FrontTraverseIndex; i < BackTraverseIndex; ++i)
		{
			TArray<FVector> SubdivisionPoint;
			TargetSpline->ConvertSplineSegmentToPolyLine(i, ESplineCoordinateSpace::Local, MaxResampleDistance,
			                                             SubdivisionPoint);
			//对于曲线样条，每一段取左闭右开
			for (int j = 0; j < SubdivisionPoint.Num() - 1; ++j)
			{
				float DisToSubdivisionPoint = TargetSpline->GetDistanceAlongSplineAtLocation(
					SubdivisionPoint[j], ESplineCoordinateSpace::Local);
				ResamplePointsOnSpline.Emplace(
					TargetSpline->GetTransformAtDistanceAlongSpline(
						DisToSubdivisionPoint, ESplineCoordinateSpace::Local, true));
			}
			//对于直线样条为双侧闭区间
			if (TargetSpline->GetSplinePointType(i) == ESplinePointType::Linear)
			{
				int NextIndex = FMath::Modulo(i + 1, TargetSpline->GetNumberOfSplinePoints());
				FTransform TransformOfSegmentEnd = TargetSpline->GetTransformAtSplinePoint(
					NextIndex, ESplineCoordinateSpace::Local, true);
				FTransform TransformOfSegmentStart = TargetSpline->GetTransformAtSplinePoint(
					i, ESplineCoordinateSpace::Local, true);
				TransformOfSegmentEnd.SetRotation(TransformOfSegmentStart.GetRotation());
				ResamplePointsOnSpline.Emplace(TransformOfSegmentEnd);
			}
		}
		//结尾Shrink的前一个控制点（n-1）
		//这个能集合两种情况FrontTraverseIndex!=BackTraverseIndex和FrontTraverseIndex==BackTraverseIndex
		FTransform TransOfLastButOneControlPoint = TargetSpline->GetTransformAtSplinePoint(
			BackTraverseIndex, ESplineCoordinateSpace::Local, true);
		if (TargetSpline->GetSplinePointType(BackTraverseIndex) == ESplinePointType::Linear)
		{
			TransOfLastButOneControlPoint.SetRotation(LastTransform.GetRotation());
		}
		ResamplePointsOnSpline.Emplace(TransOfLastButOneControlPoint);

		//结尾分段合并
		ResamplePointsOnSpline.Append(EndSegment);
	}
	OutResampledTransform = MoveTemp(ResamplePointsOnSpline);
	return true;
}
TArray<FTransform> URoadGeneratorSubsystem::GetSubdivisionBetweenGivenAndControlPoint(
	const USplineComponent* TargetSpline, float TargetLength, int32 NeighborIndex, bool bIsBackTraverse,
	float MaxResampleDistance, bool bIsClosedInterval)
{
	//函数处理首段或末段
	TArray<FTransform> ResultTransforms;
	//正序遍历给定后一个点，倒序遍历给定前一个点；输出顺序均为正序
	//对于StartShrink，使用正序遍历，情况为[跳过点...StartShrink...NeighborIndex]
	//对于EndShrink，使用倒序遍历，情况为[NeighborIndex...EndShrink...跳过点]
	int PreviousPointIndex = bIsBackTraverse ? NeighborIndex : NeighborIndex - 1;
	if (PreviousPointIndex < 0 || PreviousPointIndex >= TargetSpline->GetNumberOfSplinePoints())
	{
		return ResultTransforms;
	}
	//对于闭区间正向遍历，把ShrinkStart添加进去
	if (bIsClosedInterval && !bIsBackTraverse)
	{
		ResultTransforms.Emplace(
			TargetSpline->GetTransformAtDistanceAlongSpline(TargetLength, ESplineCoordinateSpace::Local, true));
	}
	ESplinePointType::Type PreviousPointType = TargetSpline->GetSplinePointType(PreviousPointIndex);
	if (PreviousPointType == ESplinePointType::Linear)
	{
		if (bIsClosedInterval)
			ResultTransforms.Emplace(
				TargetSpline->GetTransformAtDistanceAlongSpline(NeighborIndex, ESplineCoordinateSpace::Local, true));
	}
	else
	{
		TArray<FVector> SubdivisionLocations;
		//下面这个函数可以给相邻分段加细分采样点,包含首尾
		TargetSpline->ConvertSplineSegmentToPolyLine(PreviousPointIndex, ESplineCoordinateSpace::Local,
		                                             MaxResampleDistance, SubdivisionLocations);
		//仅保留在StartShrink之后一定距离的点
		for (int i = 0; i < SubdivisionLocations.Num(); ++i)
		{
			//非闭区间首尾点跳过
			if (!bIsClosedInterval && (i == 0 || i == SubdivisionLocations.Num() - 1))
			{
				continue;
			}
			float DisOfSubdivisionPoint = TargetSpline->GetDistanceAlongSplineAtLocation(
				SubdivisionLocations[i], ESplineCoordinateSpace::Local);
			//MaxResampleDistance目前在FLaneMeshInfo中配置
			const float ThresholdValue = bIsBackTraverse
				                             ? TargetSpline->GetSplineLength() - TargetLength + 0.25 *
				                             MaxResampleDistance
				                             : TargetLength + 0.25 * MaxResampleDistance;
			if (bIsBackTraverse)
			{
				//对于EndShrink，使用倒序遍历，情况为[NeighborIndex...<SelectedPoint>...EndShrink...<UnselectedPoint>...跳过点]
				if (DisOfSubdivisionPoint <= ThresholdValue)
				{
					ResultTransforms.Emplace(
						TargetSpline->GetTransformAtDistanceAlongSpline(
							DisOfSubdivisionPoint, ESplineCoordinateSpace::Local, true));
				}
			}
			else
			{
				//对于StartShrink，使用正序遍历，情况为[跳过点...<UnselectedPoint>...StartShrink...<SelectedPoint>...NeighborIndex]
				if (DisOfSubdivisionPoint >= ThresholdValue)
				{
					ResultTransforms.Emplace(
						TargetSpline->GetTransformAtDistanceAlongSpline(
							DisOfSubdivisionPoint, ESplineCoordinateSpace::Local, true));
				}
			}
		}
	}
	//对于闭区间，结尾段Shrink把Shrink结尾点添加进去
	if (bIsClosedInterval && bIsBackTraverse)
	{
		if (PreviousPointType == ESplinePointType::Linear)
		{
			//结尾段的End值需要偏移，特别注意Shrink=0的时候Rotation
			const float DisToNextPoint = bIsBackTraverse
				                             ? (TargetSpline->GetSplineLength() - TargetLength)
				                             : GetSplineSegmentLength(TargetSpline, NeighborIndex) - TargetLength;
			FTransform EndTransform = TargetSpline->GetTransformAtDistanceAlongSpline(
				DisToNextPoint, ESplineCoordinateSpace::Local, true);
			FRotator EndRotation = TargetSpline->GetRotationAtSplinePoint(NeighborIndex, ESplineCoordinateSpace::Local);
			EndTransform.SetRotation(UE::Math::TQuat<double>(EndRotation));
			ResultTransforms.Emplace(EndTransform);
		}
		else
		{
			const float LengthToShrinkEnd = TargetSpline->GetSplineLength() - TargetLength;
			ResultTransforms.Emplace(
				TargetSpline->GetTransformAtDistanceAlongSpline(LengthToShrinkEnd, ESplineCoordinateSpace::Local,
				                                                true));
		}
	}
	return ResultTransforms;
}

TArray<FTransform> URoadGeneratorSubsystem::GetSubdivisionOnSingleSegment(const USplineComponent* TargetSpline,
                                                                          float StartShrink, float EndShrink,
                                                                          float MaxResampleDistance,
                                                                          bool bIsClosedInterval)
{
	TArray<FTransform> ResultTransforms;
	if (nullptr == TargetSpline || TargetSpline->GetNumberOfSplinePoints() < 2)
	{
		return ResultTransforms;
	}
	//为闭合区间添加起点
	if (bIsClosedInterval)
	{
		ResultTransforms.Emplace(
			TargetSpline->GetTransformAtDistanceAlongSpline(StartShrink, ESplineCoordinateSpace::Local, true));
	}

	TArray<FVector> SubdivisionLocations;
	TargetSpline->ConvertSplineSegmentToPolyLine(0, ESplineCoordinateSpace::Local, MaxResampleDistance,
	                                             SubdivisionLocations);
	//样条线闭合时获取往复所有细分
	if (TargetSpline->IsClosedLoop())
	{
		TArray<FVector> SubdivisionLocationsBack;
		TargetSpline->ConvertSplineSegmentToPolyLine(1, ESplineCoordinateSpace::Local, MaxResampleDistance,
		                                             SubdivisionLocationsBack);
		SubdivisionLocations.Append(SubdivisionLocationsBack);
	}

	if (SubdivisionLocations.Num() > 2)
	{
		SubdivisionLocations.Reserve(SubdivisionLocations.Num() + 1);
		const float DisToShrinkEnd = TargetSpline->GetSplineLength() - EndShrink;
		const float DistanceThreshold = 50.0f;
		for (const FVector& SubdivisionLocation : SubdivisionLocations)
		{
			float DisOfSubdivisionPoint = TargetSpline->GetDistanceAlongSplineAtLocation(
				SubdivisionLocation, ESplineCoordinateSpace::Local);
			if (DisOfSubdivisionPoint > (StartShrink + DistanceThreshold) && DisOfSubdivisionPoint < (DisToShrinkEnd -
				DistanceThreshold))
			{
				ResultTransforms.Emplace(
					TargetSpline->GetTransformAtDistanceAlongSpline(DisOfSubdivisionPoint,
					                                                ESplineCoordinateSpace::Local, true));
			}
		}
	}
	//为闭合区间添加终点
	if (bIsClosedInterval)
	{
		ResultTransforms.Emplace(
			TargetSpline->GetTransformAtDistanceAlongSpline(EndShrink, ESplineCoordinateSpace::Local, true));
	}
	return ResultTransforms;
}

#pragma endregion GenerateRoad
