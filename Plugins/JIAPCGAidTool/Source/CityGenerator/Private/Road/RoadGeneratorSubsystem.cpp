// Fill out your copyright notice in the Description page of Project Settings.


#include "Road/RoadGeneratorSubsystem.h"

#include "CityGeneratorSubSystem.h"
#include "EditorComponentUtilities.h"
#include "NotifyUtilities.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/TextRenderComponent.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Road/BlockMeshGenerator.h"
#include "Road/IntersectionMeshGenerator.h"
#include "Road/RoadGeometryUtilities.h"
#include "Road/RoadGraphForBlock.h"
#include "Road/RoadMeshGenerator.h"
#include "Road/RoadSegmentStruct.h"

/*static TAutoConsoleVariable<float> PolyLineSubdivisionDis(
	TEXT("SplineToPolySampleDis"), 50.0f,TEXT("Sample Distance Convert Spline To PolyLine"), ECVF_Default);*/
static TAutoConsoleVariable<bool> AddTextRender(
	TEXT("AddTextRenderToActor"), false,TEXT("Add TextRenderComponent To Display GraphIndex"));
static TAutoConsoleVariable<bool> bEnableVisualDebug(
	TEXT("bEnableVisualDebug"), false, TEXT("Enable Graphic Debugging Shape,Include Point,Box,Sphere etal"),
	ECVF_Default);


void URoadGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ComponentMoveHandle = GEditor->OnComponentTransformChanged().AddUObject(
		this, &URoadGeneratorSubsystem::OnLevelComponentMoved);
	RoadActorRemovedHandle = ComponentMoveHandle = GEditor->OnLevelActorDeleted().AddUObject(
		this, &URoadGeneratorSubsystem::OnRoadActorRemoved);
	RoadGraph = NewObject<URoadGraph>();
	WorldChangeDelegate = GEditor->OnWorldDestroyed().AddUObject(this, &URoadGeneratorSubsystem::OnWorldChanged);
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

void URoadGeneratorSubsystem::OnWorldChanged(UWorld* World)
{
	if (RoadGraph)
	{
		RoadGraph->RemoveAllEdges();
	}
}


void URoadGeneratorSubsystem::OnRoadActorRemoved(AActor* RemovedActor)
{
	if (nullptr == RemovedActor) { return; }
	URoadMeshGenerator* RoadDataComp = Cast<URoadMeshGenerator>(
		RemovedActor->GetComponentByClass(URoadMeshGenerator::StaticClass()));
	if (nullptr != RoadDataComp)
	{
		IDToRoadGenerator.Remove(RoadDataComp->GetGlobalIndex());
		return;
	}
	UIntersectionMeshGenerator* IntersectionDataComp = Cast<UIntersectionMeshGenerator>(
		RemovedActor->GetComponentByClass(UIntersectionMeshGenerator::StaticClass()));
	if (nullptr != IntersectionDataComp)
	{
		IDToIntersectionGenerator.Remove(IntersectionDataComp->GetGlobalIndex());
		TArray<FIntersectionSegment> Segments = IntersectionDataComp->GetIntersectionSegmentsData();
		TArray<TWeakObjectPtr<USplineComponent>> InfluencedSplines;
		for (auto& Segment : Segments)
		{
			InfluencedSplines.Emplace(Segment.OwnerSpline);
		}
		for (auto& InfluencedSpline : InfluencedSplines)
		{
			if (IntersectionCompOnSpline.Contains(InfluencedSpline))
			{
				IntersectionCompOnSpline[InfluencedSpline].Remove(
					TWeakObjectPtr<UIntersectionMeshGenerator>(IntersectionDataComp));
			}
		}
		return;
	}
}

void URoadGeneratorSubsystem::Deinitialize()
{
	SplineSegmentsInfo.Empty();
	SplineQuadTree.Empty();
	IDToRoadGenerator.Empty();
	IDToIntersectionGenerator.Empty();
	GEditor->OnComponentTransformChanged().Remove(ComponentMoveHandle);
	GEditor->OnLevelActorDeleted().Remove(RoadActorRemovedHandle);
	GEditor->OnWorldDestroyed().Remove(WorldChangeDelegate);
	RoadGraph = nullptr;
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
	IDToIntersectionGenerator.Reserve(IntersectionResults.Num());
	IDToIntersectionGenerator.Reset();
	IntersectionCompOnSpline.Reset();

	TArray<FIntersectionSegment> IntersectionBuildData;
	//对每一个交点生成Actor挂载
	for (int32 i = 0; i < IntersectionResults.Num(); ++i)
	{
		//切割交点分段
		if (TearIntersectionToSegments(IntersectionResults[i], IntersectionBuildData))
		{
			if (bEnableVisualDebug.GetValueOnGameThread())
			{
				for (int32 k = 0; k < IntersectionBuildData.Num(); ++k)
				{
					FColor DebugColor = FColor((k + 1.0) / IntersectionBuildData.Num() * 255, 0, 0);
					DrawDebugSphere(UEditorComponentUtilities::GetEditorContext()->GetWorld(),
					                IntersectionBuildData[k].IntersectionEndPointWS, 100.0, 8, DebugColor, true, -1, 0,
					                10.0f);
				}
			}
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

			if (true == AddTextRender.GetValueOnGameThread())
			{
				AddDebugTextRender(IntersectionActor, FColor::Turquoise,
				                   FString::Printf(TEXT("II:%d"), GeneratorComp->GetGlobalIndex()));
			}

			IDToIntersectionGenerator.Emplace(GeneratorComp->GetGlobalIndex(), GeneratorComp);
			for (const FIntersectionSegment& BuildData : IntersectionBuildData)
			{
				if (!IntersectionCompOnSpline.Contains(BuildData.OwnerSpline))
				{
					IntersectionCompOnSpline.Emplace(BuildData.OwnerSpline,
					                                 TArray<TWeakObjectPtr<UIntersectionMeshGenerator>>());
				}
				IntersectionCompOnSpline[BuildData.OwnerSpline].Emplace(GeneratorComp);
			}
		}
	}

	FlushPersistentDebugLines(GetWorld());
	//调用生成
	for (const auto& IDGeneratorPair : IDToIntersectionGenerator)
	{
		if (!IDGeneratorPair.Value.IsValid())
		{
			continue;
		}
		IDGeneratorPair.Value->SetDrawVisualDebug(bEnableVisualDebug.GetValueOnGameThread());
		IDGeneratorPair.Value->GenerateMesh();
	}
	bIntersectionsGenerated = true;
}

void URoadGeneratorSubsystem::VisualizeSegmentByDebugline(bool bUpdateBeforeDraw, float Thickness,
                                                          bool bFlushBeforeDraw)
{
	if (bUpdateBeforeDraw)
	{
		InitialRoadSplines();
	}
	if (SplineSegmentsInfo.IsEmpty())
	{
		return;
	}
	if (bFlushBeforeDraw)
	{
		FlushPersistentDebugLines(UEditorComponentUtilities::GetEditorContext());
	}
	for (const auto& SegmentOfSingleSpline : SplineSegmentsInfo)
	{
		if (!SegmentOfSingleSpline.Key.IsValid()) { continue; }
		USplineComponent* OwnerSpline = SegmentOfSingleSpline.Key.Pin().Get();
		TArray<FSplinePolyLineSegment> Segments = SegmentOfSingleSpline.Value;
		for (int32 i = 0; i < Segments.Num(); ++i)
		{
			FColor DebugColor = Segments.Num() != 1 ? FColor(i * 255 / (Segments.Num() - 1)) : FColor(255, 255, 1);
			DrawDebugSphere(OwnerSpline->GetWorld(), Segments[i].StartTransform.GetLocation(), 100.0f, 8, DebugColor,
			                true);
			DrawDebugSphere(OwnerSpline->GetWorld(), Segments[i].EndTransform.GetLocation(), 100.0f, 8, DebugColor,
			                true);
			FVector CenterLocation = (Segments[i].StartTransform.GetLocation() + Segments[i].EndTransform.GetLocation())
				/ 2.0;
			double BoxExtendX = FVector::Dist2D(Segments[i].StartTransform.GetLocation(),
			                                    Segments[i].EndTransform.GetLocation()) / 2.0;
			FVector BoxExtend(BoxExtendX, 250.0, 100.0);
			FRotator BoxRotator = UKismetMathLibrary::MakeRotFromX((
				Segments[i].StartTransform.GetLocation() - Segments[i].EndTransform.GetLocation()).GetSafeNormal());
			DrawDebugBox(OwnerSpline->GetWorld(), CenterLocation, BoxExtend, BoxRotator.Quaternion(), DebugColor, true);
		}
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
	RoadSplines = DataSubsystem->GetSplines();
	if (RoadSplines.IsEmpty())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Error::Find Null Spline");
		return false;
	}
	bIntersectionsGenerated = false;
	FSplinePolyLineSegment::ResetGlobalIndex();
	for (TWeakObjectPtr<USplineComponent> SplineComponent : RoadSplines)
	{
		USplineComponent* PinnedSplineComp = SplineComponent.Pin().Get();
		//CVar细分预览
		//PolyLineSubdivisionDis.GetValueOnGameThread()
		UpdateSplineSegments(PinnedSplineComp);
	}
	//
	SplineQuadTree.Empty();
	return true;
}

void URoadGeneratorSubsystem::UpdateSplineSegments(USplineComponent* TargetSpline)
{
	if (nullptr == TargetSpline)
	{
		return;
	}
	TArray<FTransform> ResamplePointsOnSpline;
	const int32 OriginalSegmentCount = TargetSpline->GetNumberOfSplineSegments();
	//const float OriginalSplineLength = TargetSpline->GetSplineLength();
	//没构成有效样条
	if (OriginalSegmentCount < 1)
	{
		return;
	}
	ResamplePointsOnSpline.Append(ResampleSpline(TargetSpline));

	if (ResamplePointsOnSpline.IsEmpty())
	{
		ensureAlwaysMsgf(!ResamplePointsOnSpline.IsEmpty(), TEXT("Get Empty PolyPointArray"));
		return;
	}
	TArray<FSplinePolyLineSegment> Segments;
	const int32 SegmentCount = ResamplePointsOnSpline.Num() - 1;
	Segments.Reserve(SegmentCount);

	for (int32 i = 1; i < ResamplePointsOnSpline.Num(); i++)
	{
		Segments.Emplace(FSplinePolyLineSegment(TargetSpline, i - 1, SegmentCount - 1, ResamplePointsOnSpline[i - 1],
		                                        ResamplePointsOnSpline[i]));
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
	//计算世界包围盒，使用ForceInit可以在后续追加包围盒时自动计算总包围盒大小

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
				const uint32 IndexGap = AllSegments[i].SegmentIndex > OverlappedSegment.SegmentIndex
					                        ? AllSegments[i].SegmentIndex - OverlappedSegment.SegmentIndex
					                        : OverlappedSegment.SegmentIndex - AllSegments[i].SegmentIndex;
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
				TArray<uint32> IntersectedSegmentIndex{AllSegments[i].SegmentIndex, OverlappedSegment.SegmentIndex};
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
			FRotator FlowOutPointRot = TargetSpline->GetRotationAtDistanceAlongSpline(
				DistanceOfNextPoint, ESplineCoordinateSpace::World);
			OutSegments.Emplace(FIntersectionSegment(IntersectedSplines[i], FlowOutPointLoc, FlowOutPointRot, false,
			                                         500.0f));
		}
		//判断前边一段是不是在样条上
		if (Distance > UniformDistance)
		{
			FVector FlowInPointLoc = TargetSpline->GetLocationAtDistanceAlongSpline(
				Distance - UniformDistance, ESplineCoordinateSpace::World);
			FRotator FlowInPointRot = TargetSpline->GetRotationAtDistanceAlongSpline(
				DistanceOfNextPoint, ESplineCoordinateSpace::World);
			OutSegments.Emplace(FIntersectionSegment(IntersectedSplines[i], FlowInPointLoc, FlowInPointRot, true,
			                                         500.0f));
		}
	}
	if (OutSegments.IsEmpty())
	{
		return false;
	}
	//根据顺时针顺序排序，X正方向为0，Y正方向为正
	FVector IntersectionPoint = InIntersectionInfo.WorldLocation;
	UE_LOG(LogTemp, Display, TEXT("Begin Sort"));
	OutSegments.Sort([&IntersectionPoint](const FIntersectionSegment& A, const FIntersectionSegment& B)
	{
		FVector ProjectedA = FVector::VectorPlaneProject((A.IntersectionEndPointWS - IntersectionPoint),
		                                                 FVector::UnitZ());
		FVector2D RelA(ProjectedA);
		FVector ProjectedB = FVector::VectorPlaneProject((B.IntersectionEndPointWS - IntersectionPoint),
		                                                 FVector::UnitZ());
		FVector2D RelB(ProjectedB);

		float AngleA = FMath::Atan2(RelA.Y, RelA.X);
		float AngleB = FMath::Atan2(RelB.Y, RelB.X);

		UE_LOG(LogTemp, Display, TEXT("AngleA: %f,AngleB: %f"), AngleA, AngleB);
		// Atan2返回范围为[-π,π)转换为[0, 2π)范围
		if (AngleA < 0) AngleA += 2 * PI;
		if (AngleB < 0) AngleB += 2 * PI;

		if (AngleA != AngleB)
		{
			float AngleAInDegree = FMath::RadiansToDegrees(AngleA);
			float AngleBInDegree = FMath::RadiansToDegrees(AngleB);
			UE_LOG(LogTemp, Display, TEXT("AngleA: %f FromLoc:%s,AngleB: %f FromLoc:%s"), AngleAInDegree,
			       *A.IntersectionEndPointWS.ToString(), AngleBInDegree, *B.IntersectionEndPointWS.ToString());
			return AngleA < AngleB; // 极角小的排在前面
		}
		else
		{
			// 角度相同，按距离排序（近的在前）
			return RelA.SizeSquared() < RelB.SizeSquared();
		}
	});
	UE_LOG(LogTemp, Display, TEXT("End Sort"));
	return true;
}

#pragma endregion GenerateIntersection

#pragma region GenerateRoad
TArray<FSplinePolyLineSegment> URoadGeneratorSubsystem::GetInteractionOccupiedSegments(
	TWeakObjectPtr<UIntersectionMeshGenerator> TargetIntersection) const
{
	TArray<FSplinePolyLineSegment> OverlappedSegments;
	if (TargetIntersection.IsValid())
	{
		UIntersectionMeshGenerator* IntersectionMeshGenerator = TargetIntersection.Pin().Get();
		FBox2D BoxOfIntersection = IntersectionMeshGenerator->GetOccupiedBox();
		SplineQuadTree.GetElements(BoxOfIntersection, OverlappedSegments);
	}
	return OverlappedSegments;
}

void URoadGeneratorSubsystem::GenerateRoads()
{
	uint32 RoadCounter = 0;
	//如果Intersection已经生成则按照之前的信息
	if (!bIntersectionsGenerated && bNeedRefreshSegmentData)
	{
		EAppReturnType::Type UserChoice = UNotifyUtilities::ShowMsgDialog(
			EAppMsgType::OkCancel, "Please Generate Intersection First,Click [OK] To Generate Intersection", true);
		//InitialRoadSplines();
		if (UserChoice == EAppReturnType::Ok)
		{
			AsyncTask(ENamedThreads::GameThread, [this]()
			{
				this->GenerateIntersections();
			});
		}
		return;
	}
	if (IntersectionCompOnSpline.IsEmpty() || SplineSegmentsInfo.IsEmpty())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Error:Find Null Spline");
		return;
	}
	//1，需要把完整、连续的Spline提取出来，使用四叉树提取可能存在十字路口的格子
	for (const auto& SingleSpline : RoadSplines)
	{
		if (!SingleSpline.IsValid())
		{
			continue;
		}
		//找到所有Segment，然后从其中移除被交叉路口占用的

		//Spline上的全部Segments
		TArray<FSplinePolyLineSegment> AllSegmentOnSpline;
		TMap<uint32, FSplinePolyLineSegment> IndexToAllSegments;
		//在这里获得的值是有序的
		TArray<uint32> AllSegmentsIndex;
		//IndexToAllSegments.Reserve(AllSegmentOnSpline.Num());
		if (SplineSegmentsInfo.Contains(SingleSpline))
		{
			AllSegmentOnSpline = SplineSegmentsInfo[SingleSpline];
			IndexToAllSegments.Reserve(AllSegmentOnSpline.Num());
			AllSegmentsIndex.Reserve(AllSegmentOnSpline.Num());
			for (const auto& Segment : AllSegmentOnSpline)
			{
				IndexToAllSegments.Emplace(Segment.GetGlobalIndex(), Segment);
				AllSegmentsIndex.Emplace(Segment.GetGlobalIndex());
			}
		}
		//取交汇路口占用的，同时把交接路口取出来
		//记录交汇路口占据的Segment编号
		TArray<uint32> OccupiedSegmentsIndex;
		//记录交汇路口和道路的连接点信息
		TArray<FIntersectionSegment> RoadIntersectionConnectionInfo;
		if (IntersectionCompOnSpline.Contains(SingleSpline))
		{
			TSet<TWeakObjectPtr<UIntersectionMeshGenerator>> IntersectionGensOnSpline =
				IntersectionCompOnSpline[SingleSpline];
			for (const TWeakObjectPtr<UIntersectionMeshGenerator>& MeshGenerator : IntersectionGensOnSpline)
			{
				if (!MeshGenerator.IsValid()) { continue; }
				UIntersectionMeshGenerator* MeshGeneratorPtr = MeshGenerator.Pin().Get();
				TArray<FSplinePolyLineSegment> PotentialIntersections;
				SplineQuadTree.GetElements(MeshGeneratorPtr->GetOccupiedBox(), PotentialIntersections);
				for (const FSplinePolyLineSegment& PolyLineSegment : PotentialIntersections)
				{
					if (SingleSpline == PolyLineSegment.OwnerSpline)
					{
						OccupiedSegmentsIndex.Emplace(PolyLineSegment.GetGlobalIndex());
					}
				}
				//所在样条上**所有**的交叉坐标
				RoadIntersectionConnectionInfo.Append(MeshGeneratorPtr->GetRoadConnectionPoint(SingleSpline));
			}
		}
		//切分出的连续分段Segment组
		TArray<TArray<uint32>> ContinuousSegmentsGroups;
		//去除被占用的，获取连续的Segments数据,均使用GlobalID作为区分
		if (!OccupiedSegmentsIndex.IsEmpty())
		{
			ContinuousSegmentsGroups = GetContinuousIndexSeries(AllSegmentsIndex, OccupiedSegmentsIndex);
		}
		else
		{
			ContinuousSegmentsGroups.Emplace(AllSegmentsIndex);
		}

		//2.判断十字路口端点位于哪个Segment、将其作为附加信息与连续Segments封装到FConnectionInsertInfo结构体
		//对应ContinuousSegmentsGroup的二维序号和是否为前缀
		TMultiMap<int32, FConnectionInsertInfo> SegmentGroupToConnectionToHead;
		USplineComponent* TargetSplinePtr = SingleSpline.Pin().Get();
		for (const FIntersectionSegment& IntersectionSegment : RoadIntersectionConnectionInfo)
		{
			FBox2D BoxOfConnection(ForceInit);
			BoxOfConnection += FVector2D(IntersectionSegment.IntersectionEndPointWS);
			//这个值比较重要，太小可能搜不到相邻节点，太大要筛选的量过多
			BoxOfConnection = BoxOfConnection.ExpandBy(50.0f);
			TArray<FSplinePolyLineSegment> PotentialConnection;
			SplineQuadTree.GetElements(BoxOfConnection, PotentialConnection);
			//部分路口外部无衔接道路
			if (PotentialConnection.IsEmpty())
			{
				if (bEnableVisualDebug.GetValueOnGameThread())
				{
					DrawDebugBox(UEditorComponentUtilities::GetEditorContext()->GetWorld(),
					             IntersectionSegment.IntersectionEndPointWS, FVector(10.0f), FColor::Red, true, -1, 0,
					             5.0f);
				}
				continue;
			}
			//有多个可能性，根据距离判定究竟属于哪个Segment，放到PotentialConnection[0]
			if (PotentialConnection.Num() != 1)
			{
				uint32 OwnerSegmentIndex = 0;
				float MinDistance = FLT_MAX;
				for (int i = 0; i < PotentialConnection.Num(); ++i)
				{
					FVector SegmentCenter = PotentialConnection[i].StartTransform.GetLocation() + PotentialConnection[i]
						.EndTransform.GetLocation();
					float DisCenterToConnection = FVector::DistSquared2D(
						SegmentCenter, IntersectionSegment.IntersectionEndPointWS);
					if (DisCenterToConnection < MinDistance)
					{
						OwnerSegmentIndex = i;
					}
				}
				if (OwnerSegmentIndex != 0)
				{
					PotentialConnection.Swap(0, OwnerSegmentIndex);
				}
			}
			//所属Segment保存在0位置
			FConnectionInsertInfo InsertInfo = FindInsertIndexInExistedContinuousSegments(
				ContinuousSegmentsGroups, AllSegmentOnSpline,
				PotentialConnection[0].GetGlobalIndex(), IntersectionSegment.IntersectionEndPointWS);
			InsertInfo.IntersectionGlobalIndex = IntersectionSegment.OwnerGlobalIndex;
			//传递顺时针排序给建图用
			InsertInfo.EntryLocalIndex = IntersectionSegment.EntryLocalIndex;
			float DisOfConnectionOnSpline = TargetSplinePtr->GetDistanceAlongSplineAtLocation(
				IntersectionSegment.IntersectionEndPointWS, ESplineCoordinateSpace::World);
			FTransform ConnectionTransform = TargetSplinePtr->GetTransformAtDistanceAlongSpline(
				DisOfConnectionOnSpline, ESplineCoordinateSpace::World);

			ConnectionTransform.SetRotation(IntersectionSegment.IntersectionEndRotWS.Quaternion());

			InsertInfo.ConnectionTrans = ConnectionTransform;
			SegmentGroupToConnectionToHead.Emplace(
				InsertInfo.GroupIndex, InsertInfo);
			//不要直接在这里插入（相当于一边遍历一边修改），会破坏上面的算法
		}

		//如果是闭合曲线把Segments合并
		if (SingleSpline->IsClosedLoop())
		{
			if (ContinuousSegmentsGroups.Num() > 1)
			{
				bool bCanMerge = (AllSegmentOnSpline[0].GetGlobalIndex() == ContinuousSegmentsGroups[0][0]);
				bCanMerge &= (AllSegmentOnSpline.Last(0).GetGlobalIndex() == ContinuousSegmentsGroups.Last(0).Last(0));

				if (bCanMerge)
				{
					//合并到第一组，可以避免后续组的去除
					TArray<uint32> NewFirstGroup;
					int32 LastGroupLength = ContinuousSegmentsGroups.Last(0).Num();
					NewFirstGroup.SetNum(ContinuousSegmentsGroups[0].Num() + ContinuousSegmentsGroups.Last(0).Num());
					FMemory::Memcpy(NewFirstGroup.GetData(), ContinuousSegmentsGroups.Last(0).GetData(),
					                LastGroupLength * sizeof(uint32));
					FMemory::Memcpy(NewFirstGroup.GetData() + LastGroupLength,
					                ContinuousSegmentsGroups[0].GetData(),
					                ContinuousSegmentsGroups[0].Num() * sizeof(uint32));
					ContinuousSegmentsGroups[0] = NewFirstGroup;
					//调整衔接内容,能衔接的情况必然是第一组元素去除尾部或最后一组元素去除头部；因为是把最后一组元素合并到头部，所以只需要移动一组
					if (SegmentGroupToConnectionToHead.Contains(ContinuousSegmentsGroups.Num() - 1))
					{
						TArray<FConnectionInsertInfo> HeadInsertsOfLastGroup;
						SegmentGroupToConnectionToHead.MultiFind(ContinuousSegmentsGroups.Num() - 1,
						                                         HeadInsertsOfLastGroup);
						//插入元素必然在头部，理论上应该只有一个元素
						for (auto& Insert : HeadInsertsOfLastGroup)
						{
							Insert.GroupIndex = 0;
							SegmentGroupToConnectionToHead.Emplace(0, Insert);
							ensureAlwaysMsgf(Insert.bConnectToGroupHead==true, TEXT("Error Insert Place,Please Check"));
						}
						SegmentGroupToConnectionToHead.Remove(ContinuousSegmentsGroups.Num() - 1);
					}
					//移除最后一组
					ContinuousSegmentsGroups.RemoveAt(ContinuousSegmentsGroups.Num() - 1);
				}
			}
		}

		//3.创建Actor负载信息
		for (int32 i = 0; i < ContinuousSegmentsGroups.Num(); ++i)
		{
			TArray<uint32>& ContinuousSegments = ContinuousSegmentsGroups[i];
			//把所有Segments创建路径
			TArray<FTransform> RoadSegmentTransforms;
			FTransform StartTransform = IndexToAllSegments[ContinuousSegments[0]].StartTransform;
			RoadSegmentTransforms.Emplace(StartTransform);
			for (int32 j = 0; j < ContinuousSegments.Num(); ++j)
			{
				RoadSegmentTransforms.Emplace(IndexToAllSegments[ContinuousSegments[j]].EndTransform);
			}
			//生成衔接位置信息,用Transform初始化结构体
			FRoadSegmentsGroup RoadWithConnectInfo(RoadSegmentTransforms);
			TArray<int32> ConnectedIntersections;
			ConnectedIntersections.Init(INT32_ERROR, 2);
			//连接到Intersection的路口序号，用于图邻接表构建
			TArray<int32> EntryIndexOfIntersections;
			EntryIndexOfIntersections.Init(INT32_ERROR, 2);
			if (SegmentGroupToConnectionToHead.Contains(i))
			{
				TArray<FConnectionInsertInfo> Connections;
				SegmentGroupToConnectionToHead.MultiFind(i, Connections);
				if (!Connections.IsEmpty())
				{
					for (const auto& Connection : Connections)
					{
						if (Connection.bConnectToGroupHead == true)
						{
							RoadWithConnectInfo.bHasHeadConnection = true;
							RoadWithConnectInfo.HeadConnectionTrans = Connection.ConnectionTrans;
							ConnectedIntersections[0] = Connection.IntersectionGlobalIndex;
							//给连接信息加负载，用于判断走向
							RoadWithConnectInfo.FromIntersectionIndex = Connection.IntersectionGlobalIndex;
							EntryIndexOfIntersections[0] = Connection.EntryLocalIndex;
						}
						else
						{
							RoadWithConnectInfo.bHasTailConnection = true;
							RoadWithConnectInfo.TailConnectionTrans = Connection.ConnectionTrans;
							ConnectedIntersections[1] = Connection.IntersectionGlobalIndex;
							//给连接信息加负载，用于判断走向
							RoadWithConnectInfo.ToIntersectionIndex = Connection.IntersectionGlobalIndex;
							EntryIndexOfIntersections[1] = Connection.EntryLocalIndex;
						}
					}
				}
			}

			FString ActorLabel = FString::Printf(TEXT("RoadActor%d"), RoadCounter);
			AActor* RoadActor = UEditorComponentUtilities::SpawnEmptyActor(ActorLabel, StartTransform);
			ensureAlways(nullptr!=RoadActor);

			UActorComponent* MeshCompTemp = UEditorComponentUtilities::AddComponentInEditor(
				RoadActor, UDynamicMeshComponent::StaticClass());
			UDynamicMeshComponent* MeshComp = Cast<UDynamicMeshComponent>(MeshCompTemp);
			UActorComponent* GeneratorCompTemp = UEditorComponentUtilities::AddComponentInEditor(
				RoadActor, URoadMeshGenerator::StaticClass());
			URoadMeshGenerator* GeneratorComp = Cast<URoadMeshGenerator>(GeneratorCompTemp);

			GeneratorComp->SetMeshComponent(MeshComp);
			GeneratorComp->SetReferenceSpline(SingleSpline);
			GeneratorComp->SetRoadInfo(RoadWithConnectInfo);
			IDToRoadGenerator.Emplace(GeneratorComp->GetGlobalIndex(), GeneratorComp);
			RoadCounter++;

			if (true == AddTextRender.GetValueOnGameThread())
			{
				AddDebugTextRender(RoadActor, FColor::Yellow,
				                   FString::Printf(TEXT("RI:%d"), GeneratorComp->GetGlobalIndex()));
			}
			//把对道路和附属节点加入图
			if (nullptr != RoadGraph)
			{
				/*RoadGraph->AddUndirectedEdge(ConnectedIntersections[0], ConnectedIntersections[1],
				                             GeneratorComp->GetGlobalIndex());*/
				RoadGraph->AddEdgeInGivenSlot(ConnectedIntersections[0], ConnectedIntersections[1],
				                              GeneratorComp->GetGlobalIndex(), EntryIndexOfIntersections[0]);
				RoadGraph->AddEdgeInGivenSlot(ConnectedIntersections[1], ConnectedIntersections[0],
				                              GeneratorComp->GetGlobalIndex(), EntryIndexOfIntersections[1]);
			}
		}
	}
	RoadGraph->PrintConnectionToLog();
	//4.调用生成
	for (const auto& IDGeneratorPair : IDToRoadGenerator)
	{
		if (!IDGeneratorPair.Value.IsValid())
		{
			continue;
		}
		IDGeneratorPair.Value->SetDrawVisualDebug(bEnableVisualDebug.GetValueOnGameThread());
		IDGeneratorPair.Value->GenerateMesh();
	}
}

TArray<TArray<uint32>> URoadGeneratorSubsystem::GetContinuousIndexSeries(const TArray<uint32>& AllSegmentIndex,
                                                                         TArray<uint32>& BreakPoints)
{
	TArray<TArray<uint32>> Results;
	if (BreakPoints.IsEmpty())
	{
		Results.Emplace(AllSegmentIndex);
		return Results;
	}
	//连续数组问题使用滑动窗口处理，但因为不需要长度数据，可以直接用单指针模拟窗口
	TArray<uint32> IndexSeries;
	//int32 LeftIndex = 0;
	int32 RightIndex = 0;
	BreakPoints.Sort();
	int32 BreakpointIndex = 0;
	int32 ValidBreakPoints = BreakPoints.Num();
	//对齐数据
	while (ValidBreakPoints > 0 && BreakPoints[BreakpointIndex] < AllSegmentIndex[0])
	{
		BreakpointIndex++;
		ValidBreakPoints--;
	}
	int32 BreakpointIndexLast = BreakPoints.Num() - 1;
	while (ValidBreakPoints > 0 && BreakPoints[BreakpointIndexLast] > AllSegmentIndex.Last(0))
	{
		BreakpointIndexLast--;
		ValidBreakPoints--;
	}
	if (ValidBreakPoints <= 0)
	{
		Results.Emplace(AllSegmentIndex);
		return Results;
	}
	while (RightIndex < AllSegmentIndex.Num())
	{
		//没遇到BreakPionts中的值是扩张
		if (AllSegmentIndex[RightIndex] != BreakPoints[BreakpointIndex])
		{
			IndexSeries.Emplace(AllSegmentIndex[RightIndex]);
			RightIndex++;
		}
		//遇到BreakPoints时更新结果并收缩
		else
		{
			if (IndexSeries.Num() > 0)
			{
				Results.Emplace(IndexSeries);
			}
			IndexSeries.Reset();
			//这里使用两个数组单调且元素不重复特性
			while (RightIndex < AllSegmentIndex.Num() &&
				BreakpointIndex < BreakPoints.Num() && AllSegmentIndex[RightIndex] == BreakPoints[BreakpointIndex])
			{
				BreakpointIndex++;
				RightIndex++;
			}
			//两个都走到头了
			if (BreakpointIndex == BreakPoints.Num() && RightIndex == AllSegmentIndex.Num())
			{
				break;
			}
			// BreakPoint是Allsegment子集，不存在BreakpointIndex没到头但是RightIndex到头的情况
			if (BreakpointIndex == BreakPoints.Num() && RightIndex < AllSegmentIndex.Num())
			{
				//RightIndex++;
				//没有发现截取子数组的函数，使用FMemory::Memcpy()
				IndexSeries.SetNum(AllSegmentIndex.Num() - RightIndex);
				FMemory::Memcpy(IndexSeries.GetData(),
				                AllSegmentIndex.GetData() + RightIndex,
				                (AllSegmentIndex.Num() - RightIndex) * sizeof(uint32));
				break;
			}
			//此时回归正常情况两者指向数字不同，收缩窗口
			//Left=Right
		}
	}
	//把最后一组数字放进去
	if (!IndexSeries.IsEmpty())
	{
		Results.Emplace(IndexSeries);
	}
	return Results;
}

FConnectionInsertInfo URoadGeneratorSubsystem::FindInsertIndexInExistedContinuousSegments(
	const TArray<TArray<uint32>>& InContinuousSegmentsGroups,
	const TArray<FSplinePolyLineSegment>& InAllSegmentOnSpline,
	const uint32 OwnerSegmentID, const FVector& PointTransWS)
{
	FConnectionInsertInfo Result;
	//利用连续特性,寻找是不是在端点
	if (OwnerSegmentID < InContinuousSegmentsGroups[0][0])
	{
		Result.GroupIndex = 0;
		Result.bConnectToGroupHead = true;
	}
	else if (OwnerSegmentID > InContinuousSegmentsGroups.Last(0).Last(0))
	{
		Result.GroupIndex = InContinuousSegmentsGroups.Num() - 1;
		Result.bConnectToGroupHead = false;
	}
	else
	{
		int32 IndexDistance = INT_MAX;
		uint32 NeighborSegmentIndex = UINT_MAX;
		bool bChoiceHead = true;
		for (int32 i = 1; i < InContinuousSegmentsGroups.Num(); ++i)
		{
			//@TODO：可以使用二分查找优化
			//遍历寻找中间位置
			if (InContinuousSegmentsGroups[i - 1].Last() < OwnerSegmentID && OwnerSegmentID <=
				InContinuousSegmentsGroups[i][0])
			{
				uint32 IndexGapToLastEnd = OwnerSegmentID - InContinuousSegmentsGroups[i - 1].Last();
				uint32 IndexGapToNextStart = InContinuousSegmentsGroups[i][0] - OwnerSegmentID;
				//距离两端序号距离一样
				if (IndexGapToLastEnd == IndexGapToNextStart)
				{
					//@TODO:这里触发偶发断言内层返回的序号不在InAllSegmentOnSpline数组中，可能是由于重复生成，待后续排查
					FVector LocOfLastEnd = InAllSegmentOnSpline[InContinuousSegmentsGroups[i - 1].Last()].EndTransform.
						GetLocation();
					float DisToLastEnd = FVector::DistSquared2D(LocOfLastEnd, PointTransWS);
					FVector LocOfNextStart = InAllSegmentOnSpline[InContinuousSegmentsGroups[i][0]].EndTransform.
						GetLocation();
					float DisToNestStart = FVector::DistSquared2D(LocOfNextStart, PointTransWS);
					//理论上不存在等于
					if (DisToLastEnd <= DisToNestStart)
					{
						Result.GroupIndex = i - 1;
						Result.bConnectToGroupHead = false;
						break;
					}
					else
					{
						Result.GroupIndex = i;
						Result.bConnectToGroupHead = true;
						break;
					}
				}
				//距离上一段终点更近
				else if (IndexGapToLastEnd < IndexGapToNextStart)
				{
					Result.GroupIndex = i - 1;
					Result.bConnectToGroupHead = false;
					break;
				}
				//距离当前段起点更近
				else
				{
					Result.GroupIndex = i;
					Result.bConnectToGroupHead = true;
					break;
				}
			}
		}
	}
	return Result;
}


TArray<FTransform> URoadGeneratorSubsystem::ResampleSpline(const USplineComponent* TargetSpline)
{
	TArray<FTransform> Results;
	if (nullptr == TargetSpline || TargetSpline->GetNumberOfSplinePoints() <= 1)
	{
		return Results;
	}
	const float SegmentMaxDisThreshold = 10 * PolyLineSampleDistance;
	TArray<FVector> PolyLineEndPointLoc;
	TArray<double> PolyLineLengths;
	//曲线，该函数返回闭合样条返回段,Distance数组是到每一个端点处的长度（类似前缀和）,ControlPoint位置一定会有一个采样点
	TargetSpline->ConvertSplineToPolyLineWithDistances(ESplineCoordinateSpace::World, PolyLineSampleDistance,
	                                                   PolyLineEndPointLoc, PolyLineLengths);
	//先处理Linear
	TArray<int32> LinearControlPointIndexes;
	for (int32 i = 0; i < PolyLineLengths.Num(); ++i)
	{
		float InputKey = TargetSpline->GetInputKeyValueAtDistanceAlongSpline(PolyLineLengths[i]);
		if (IsIntegerInFloatFormat(InputKey))
		{
			int32 ControlPointIndex = static_cast<int32>(InputKey);
			if (TargetSpline->GetSplinePointType(ControlPointIndex) == ESplinePointType::Linear && i > 0 &&
				i < PolyLineLengths.Num() - 1)
			{
				LinearControlPointIndexes.Add(i);
			}
		}
	}

	//所有细分点的初始内容
	Results.Reserve(PolyLineEndPointLoc.Num());
	for (int i = 0; i < PolyLineLengths.Num(); ++i)
	{
		Results.Emplace(
			TargetSpline->GetTransformAtDistanceAlongSpline(PolyLineLengths[i], ESplineCoordinateSpace::World,
			                                                true));
	}

	TMap<int32, TArray<FTransform>> InterplatePointsOnControlPoints;
	TMap<int32, TArray<double>> InterplateLengthOnSpline;
	if (!LinearControlPointIndexes.IsEmpty())
	{
		for (int32& ControlPointIndex : LinearControlPointIndexes)
		{
			//值过小会因为被四叉树判定为相交，生成交汇路口时报错
			const float AdditionalSampleDistance = 4 * PolyLineSampleDistance;
			//判断和前边点的距离
			float FrontNeighbourDis = PolyLineLengths[ControlPointIndex] - PolyLineLengths[ControlPointIndex - 1];
			FTransform LastTransform = TargetSpline->GetTransformAtDistanceAlongSpline(
				PolyLineLengths[ControlPointIndex - 1], ESplineCoordinateSpace::World,
				true);

			if (FrontNeighbourDis > AdditionalSampleDistance)
			{
				LastTransform = TargetSpline->GetTransformAtDistanceAlongSpline(
					PolyLineLengths[ControlPointIndex] - AdditionalSampleDistance,
					ESplineCoordinateSpace::World,
					true);
				InterplatePointsOnControlPoints.Emplace(ControlPointIndex - 1).Add(LastTransform);
				InterplateLengthOnSpline.Emplace(ControlPointIndex - 1).Emplace(
					PolyLineLengths[ControlPointIndex] - AdditionalSampleDistance);
			}

			float NextNeighbourDis = PolyLineLengths[ControlPointIndex + 1] - PolyLineLengths[ControlPointIndex];
			FTransform NextTransform = TargetSpline->GetTransformAtDistanceAlongSpline(
				PolyLineLengths[ControlPointIndex + 1], ESplineCoordinateSpace::World,
				true);
			if (NextNeighbourDis > AdditionalSampleDistance)
			{
				NextTransform = TargetSpline->GetTransformAtDistanceAlongSpline(
					PolyLineLengths[ControlPointIndex] + AdditionalSampleDistance,
					ESplineCoordinateSpace::World,
					true);
				InterplatePointsOnControlPoints.Add(ControlPointIndex).Add(NextTransform);
				InterplateLengthOnSpline.Add(ControlPointIndex).Emplace(
					PolyLineLengths[ControlPointIndex] + AdditionalSampleDistance);
			}
			//FQuat不要使用(A+B)/2计算值不对
			FQuat AverageRotator = FQuat::Slerp(LastTransform.GetRotation(), NextTransform.GetRotation(), 0.5);
			Results[ControlPointIndex].SetRotation(AverageRotator);
			FVector RotDir = AverageRotator.Rotator().Vector();
		}
		InsertElementsAtIndex(Results, InterplatePointsOnControlPoints);
		InsertElementsAtIndex(PolyLineLengths, InterplateLengthOnSpline);
	}

	TMap<int32, TArray<FTransform>> SegmentsToSubdivide;
	//检测上面函数返回的分段长度是否符合设定要求，如果不符合则记录位置进一步处理
	for (int i = 1; i < Results.Num(); ++i)
	{
		if (PolyLineLengths[i] - PolyLineLengths[i - 1] > SegmentMaxDisThreshold)
		{
			//以该点为起点的位置需要插入元素
			SegmentsToSubdivide.Emplace(i - 1);
		}
	}
	if (SegmentsToSubdivide.IsEmpty())
	{
		return Results;
	}
	//如果需要处理，在记录的位置增加细分
	for (TPair<int32, TArray<FTransform>>& TargetSegment : SegmentsToSubdivide)
	{
		const int32 SegmentIndex = TargetSegment.Key;
		const float OriginalSegmentLength = PolyLineLengths[SegmentIndex + 1] - PolyLineLengths[SegmentIndex];
		int32 TargetSubdivisionNum = FMath::CeilToInt32(OriginalSegmentLength / SegmentMaxDisThreshold);
		double TargetSubdivisionLength = OriginalSegmentLength / TargetSubdivisionNum;
		TArray<FTransform> SubdivisionPoints;
		for (int32 j = 1; j < TargetSubdivisionNum; j++)
		{
			float DisToSubdivisionPoint = static_cast<float>(j * TargetSubdivisionLength + PolyLineLengths[
				SegmentIndex]);
			TargetSegment.Value.Add(
				TargetSpline->GetTransformAtDistanceAlongSpline(DisToSubdivisionPoint, ESplineCoordinateSpace::World));
		}
	}
	//FTransform为非POD对象，不能直接内存拷贝
	InsertElementsAtIndex(Results, SegmentsToSubdivide);
	return Results;
}

#pragma endregion GenerateRoad

void URoadGeneratorSubsystem::AddDebugTextRender(AActor* TargetActor, const FColor& TextColor, const FString& Text)
{
	UActorComponent* IndexTexRenderTemp = UEditorComponentUtilities::AddComponentInEditor(
		TargetActor, UTextRenderComponent::StaticClass());
	UTextRenderComponent* IndexTexRender = Cast<UTextRenderComponent>(IndexTexRenderTemp);
	IndexTexRender->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
	IndexTexRender->SetRelativeLocation(FVector(0.0, 0.0, 50.0));
	IndexTexRender->SetText(FText::FromString(Text));
	IndexTexRender->SetTextRenderColor(TextColor);
	IndexTexRender->SetWorldSize(800.0);
	IndexTexRender->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	IndexTexRender->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
}


void URoadGeneratorSubsystem::PrintGraphConnection()
{
	if (nullptr != RoadGraph)
	{
		RoadGraph->PrintConnectionToLog();
	}
}

bool URoadGeneratorSubsystem::IsIntegerInFloatFormat(float InFloatValue)
{
	return FMath::IsNearlyEqual(InFloatValue, FMath::RoundToFloat(InFloatValue));
}

void URoadGeneratorSubsystem::GenerateCityBlock()
{
	if (nullptr == RoadGraph)
	{
		return;
	}
	TArray<FBlockLinkInfo> BlockLoops = RoadGraph->GetSurfaceInGraph();
	//移除外轮廓
	RemoveInvalidLoopInline(BlockLoops);
	if (BlockLoops.Num() <= 0)
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Find Null Valid Loop");
		return;
	}
	TArray<TArray<FVector>> AllLoopPath;
	TArray<TArray<FInterpCurveVector>> AllRefsSplineGroup;
	//获取生成轮廓信息
	for (int32 i = 0; i < BlockLoops.Num(); ++i)
	{
		const TArray<int32>& RoadIndexes = BlockLoops[i].RoadIndexes;
		const TArray<int32>& IntersectionIndexes = BlockLoops[i].IntersectionIndexes;
		FColor DebugColor = FColor::MakeRandomColor();
		FString PrintStr = "";
		PrintStr += FString::Printf(TEXT("[%d]"), IntersectionIndexes.Last());
		//单个循环边组
		TArray<FVector> SingleLoopPath;
		TArray<FInterpCurveVector> SingleRoadRefGroup;
		for (int j = 0; j < RoadIndexes.Num(); ++j)
		{
			PrintStr += FString::Printf(TEXT("-(%d)-"), RoadIndexes[j]);
			TWeakObjectPtr<URoadMeshGenerator> RoadGeneratorWeak = IDToRoadGenerator[RoadIndexes[j]];
			if (!RoadGeneratorWeak.IsValid())
			{
				continue;
			}
			URoadMeshGenerator* RoadGenerator = RoadGeneratorWeak.Pin().Get();
			//道路的起点终点
			int32 RoadPathFromConnection = INT32_ERROR;
			int32 RoadPathToConnection = INT32_ERROR;
			RoadGenerator->GetConnectionOrderOfIntersection(RoadPathFromConnection, RoadPathToConnection);
			//图顺序是当前边和它的终点
			int32 GraphStartEdge = IntersectionIndexes.Last();
			if (j != 0)
			{
				GraphStartEdge = IntersectionIndexes[j - 1];
			}
			int32 GraphEndEdge = IntersectionIndexes[j];
			ensureAlways(
				(RoadPathFromConnection==GraphStartEdge||RoadPathFromConnection==GraphEndEdge)&&(RoadPathToConnection==
					GraphStartEdge||RoadPathToConnection==GraphEndEdge));
			bool bIsForwardTraverse = RoadPathFromConnection == GraphStartEdge;
			//根据道路朝向提取Location
			TArray<FVector> RoadEdgeLocArray = RoadGenerator->GetRoadEdgePoints(bIsForwardTraverse);
			SingleLoopPath.Append(RoadEdgeLocArray);
			//根据道路走向提取参考Spline
			SingleRoadRefGroup.Emplace(
				RoadGenerator->GetSplineControlPointsInRoadRange(bIsForwardTraverse, ECoordOffsetType::LEFTEDGE));
			//十字路口的衔接点
			PrintStr += FString::Printf(TEXT("[%d]"), IntersectionIndexes[j]);
			TWeakObjectPtr<UIntersectionMeshGenerator> IntersectionGeneratorWeak = IDToIntersectionGenerator[
				IntersectionIndexes[j]];
			if (!IntersectionGeneratorWeak.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("Intersection Indexes %d Was Invalid"), IntersectionIndexes[j])
				continue;
			}
			UIntersectionMeshGenerator* IntersectionGenerator = IntersectionGeneratorWeak.Pin().Get();
			//注意顺序
			int32 FromIntersection = j == 0 ? IntersectionIndexes.Last() : IntersectionIndexes[j - 1];
			int32 EntryIndex = RoadGraph->
				FindEdgeEntryIndex(IntersectionIndexes[j], FromIntersection, RoadIndexes[j]);
			if (EntryIndex == INT32_ERROR)
			{
				UE_LOG(LogTemp, Error, TEXT("Find Null Edge In Graph"))
				continue;
			}
			UE_LOG(LogTemp, Display, TEXT("Road Index %d, Intersection %d,At EntryIndex %d"), RoadIndexes[j],
			       IntersectionIndexes[j], EntryIndex);
			TArray<FVector> TransitionalPoints = IntersectionGenerator->GetTransitionalPoints(EntryIndex);
			SingleLoopPath.Append(TransitionalPoints);
			if (bEnableVisualDebug.GetValueOnGameThread())
			{
				for (const FVector& PathPoint : SingleLoopPath)
				{
					DrawDebugSphere(RoadGenerator->GetWorld(), PathPoint, 100.0f, 8, DebugColor,
					                true);
				}
			}
		}
		AllLoopPath.Emplace(SingleLoopPath);
		AllRefsSplineGroup.Emplace(SingleRoadRefGroup);
		UE_LOG(LogTemp, Display, TEXT("Block Loop:%d {%s}"), i, *PrintStr);
	}
	//生成Actor并挂载
	for (int32 i = 0; i < AllLoopPath.Num(); i++)
	{
		FString ActorLabel = FString::Printf(TEXT("BlockActor%d"), i);
		FTransform ActorTransform = FTransform::Identity;
		ActorTransform.SetLocation(AllLoopPath[i][0]);
		AActor* BlockActor = UEditorComponentUtilities::SpawnEmptyActor(ActorLabel, ActorTransform);
		ensureAlways(nullptr!=BlockActor);

		UActorComponent* MeshCompTemp = UEditorComponentUtilities::AddComponentInEditor(
			BlockActor, UDynamicMeshComponent::StaticClass());
		UDynamicMeshComponent* MeshComp = Cast<UDynamicMeshComponent>(MeshCompTemp);
		UActorComponent* GeneratorCompTemp = UEditorComponentUtilities::AddComponentInEditor(
			BlockActor, UBlockMeshGenerator::StaticClass());
		UBlockMeshGenerator* GeneratorComp = Cast<UBlockMeshGenerator>(GeneratorCompTemp);
		GeneratorComp->SetMeshComponent(MeshComp);
		GeneratorComp->SetSweepPath(AllLoopPath[i]);
		GeneratorComp->SetInnerSplinePoints(AllRefsSplineGroup[i]);
		IDToBlockGenerator.Emplace(GeneratorComp->GetGlobalIndex(), TWeakObjectPtr<UBlockMeshGenerator>(GeneratorComp));
	}
	//生成Mesh
	for (const auto& IDGeneratorPair : IDToBlockGenerator)
	{
		if (!IDGeneratorPair.Value.IsValid())
		{
			continue;
		}
		IDGeneratorPair.Value->SetDrawVisualDebug(bEnableVisualDebug.GetValueOnGameThread());
		IDGeneratorPair.Value->GenerateMesh();
	}
}

void URoadGeneratorSubsystem::RemoveInvalidLoopInline(TArray<FBlockLinkInfo>& OutBlockLoops)
{
	double MaxArea = -1.0;
	int32 MaxAreaIndex = -1;
	for (int32 i = 0; i < OutBlockLoops.Num(); ++i)
	{
		const TArray<int32>& VertexIndex = OutBlockLoops[i].IntersectionIndexes;
		TArray<FVector2D> VertexLoc2D;
		VertexLoc2D.Reserve(VertexIndex.Num());
		for (const auto& Index : VertexIndex)
		{
			ensureAlways(IDToIntersectionGenerator.Contains(Index));
			TWeakObjectPtr<UIntersectionMeshGenerator> MeshGenerator = IDToIntersectionGenerator[Index];
			if (!MeshGenerator.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("Intersection Index:%d Is Not Valid"), Index);
				continue;
			}
			UIntersectionMeshGenerator* IntersectionComp = MeshGenerator.Pin().Get();
			VertexLoc2D.Emplace(FVector2D(IntersectionComp->GetOwner()->GetActorLocation()));
		}
		double LoopArea = URoadGeometryUtilities::GetAreaOfSortedPoints(VertexLoc2D);
		if (LoopArea > MaxArea)
		{
			MaxArea = LoopArea;
			MaxAreaIndex = i;
		}
	}
	if (OutBlockLoops.IsValidIndex(MaxAreaIndex))
	{
		OutBlockLoops.RemoveAtSwap(MaxAreaIndex);
	}
	return;
}

# pragma region DOF
/*
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
				GetSubdivisionOnSingleSegment(TargetSpline, StartShrink, EndShrink, MaxResampleDistance, true, true));
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
*/

/*void URoadGeneratorSubsystem::GenerateSingleRoadBySweep(USplineComponent* TargetSpline,
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
	RoadDataComp->SetRoadPathTransform(SweepPath);
	RoadDataComp->SetReferenceSpline(TargetSpline);
	TArray<FVector2D> SweepShape = RoadPresetMap[LaneTypeEnum].CrossSectionCoord;
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(DynamicMesh, GeometryScriptOptions,
	                                                                  SweepMeshTrans, SweepShape, SweepPath);
	UGeometryScriptLibrary_MeshNormalsFunctions::AutoRepairNormals(DynamicMesh);
	FGeometryScriptSplitNormalsOptions SplitOptions;
	FGeometryScriptCalculateNormalsOptions CalculateOptions;
	UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(DynamicMesh, SplitOptions, CalculateOptions);
}*/

/*TArray<FTransform> URoadGeneratorSubsystem::GetSubdivisionBetweenGivenAndControlPoint(
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
}*/

/*TArray<FTransform> URoadGeneratorSubsystem::GetSubdivisionOnSingleSegment(const USplineComponent* TargetSpline,
                                                                          float StartShrink, float EndShrink,
                                                                          float MaxResampleDistance,
                                                                          bool bIsClosedInterval, bool bIsLocalSpace)
{
	TArray<FTransform> ResultTransforms;
	if (nullptr == TargetSpline || TargetSpline->GetNumberOfSplinePoints() < 2)
	{
		return ResultTransforms;
	}
	const ESplineCoordinateSpace::Type CoordSpace = bIsLocalSpace
		                                                ? ESplineCoordinateSpace::Local
		                                                : ESplineCoordinateSpace::World;
	//为闭合区间添加起点
	if (bIsClosedInterval)
	{
		ResultTransforms.Emplace(
			TargetSpline->GetTransformAtDistanceAlongSpline(StartShrink, CoordSpace, true));
	}

	TArray<FVector> SubdivisionLocations;
	TargetSpline->ConvertSplineSegmentToPolyLine(0, CoordSpace, MaxResampleDistance,
	                                             SubdivisionLocations);
	//样条线闭合时获取往复所有细分
	if (TargetSpline->IsClosedLoop())
	{
		TArray<FVector> SubdivisionLocationsBack;
		TargetSpline->ConvertSplineSegmentToPolyLine(1, CoordSpace, MaxResampleDistance,
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
				SubdivisionLocation, CoordSpace);
			if (DisOfSubdivisionPoint > (StartShrink + DistanceThreshold) && DisOfSubdivisionPoint < (DisToShrinkEnd -
				DistanceThreshold))
			{
				ResultTransforms.Emplace(
					TargetSpline->GetTransformAtDistanceAlongSpline(DisOfSubdivisionPoint,
					                                                CoordSpace, true));
			}
		}
	}
	//为闭合区间添加终点
	if (bIsClosedInterval)
	{
		ResultTransforms.Emplace(
			TargetSpline->GetTransformAtDistanceAlongSpline(EndShrink, CoordSpace, true));
	}
	return ResultTransforms;
}*/

/*
float URoadGeneratorSubsystem::GetSplineSegmentLength(const USplineComponent* TargetSpline, int32 SegmentIndex)
{
	if (SegmentIndex < 0 || SegmentIndex >= TargetSpline->GetNumberOfSplineSegments())
	{
		return 0;
	}
	if (SegmentIndex == TargetSpline->GetNumberOfSplinePoints() - 1)
	{
		if (TargetSpline->IsClosedLoop())
		{
			return TargetSpline->GetSplineLength() - TargetSpline->
				GetDistanceAlongSplineAtSplinePoint(SegmentIndex + 1);
		}
		else
		{
			return 0;
		}
	}
	return TargetSpline->GetDistanceAlongSplineAtSplinePoint(SegmentIndex + 1) - TargetSpline->
		GetDistanceAlongSplineAtSplinePoint(SegmentIndex);
}
*/
# pragma endregion DOF
