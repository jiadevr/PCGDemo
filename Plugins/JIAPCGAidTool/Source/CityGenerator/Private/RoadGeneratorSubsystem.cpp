// Fill out your copyright notice in the Description page of Project Settings.


#include "RoadGeneratorSubsystem.h"

#include "EditorComponentUtilities.h"
#include "NotifyUtilities.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SplineComponent.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

#pragma region GenerateRoad

void URoadGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	//双向四车道是2*7.5米，双向六车道是2*11.25米
	RoadPresetMap.Emplace(ELaneType::SingleWay, FLaneMeshInfo(400.0, 20.0));
	RoadPresetMap.Emplace(ELaneType::TwoLaneTwoWay, FLaneMeshInfo(700.0, 20.0));
}


void URoadGeneratorSubsystem::GenerateSingleRoadBySweep(const USplineComponent* TargetSpline,
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
	ResampleSamplePoint(TargetSpline, SweepPath, RoadPresetMap[LaneTypeEnum].SampleLength, StartShrink, EndShrink);
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
			//返回开区间。起始端点在前边加入进去了，但Shrink后一个点没有加进去
			TArray<FTransform> StartSegment = GetSubdivisionBetweenGivenAndControlPoint(
				TargetSpline, StartShrink, FrontTraverseIndex, false,
				MaxResampleDistance, false);
			ResamplePointsOnSpline.Append(StartSegment);
		}
		//处理最后一个点;数组中间插入性能不好，这里额外存了变量
		//闭合曲线在这里返回的Index值有差别
		TArray<FTransform> EndSegment;
		int32 BackTraverseIndex = TargetSpline->IsClosedLoop()?OriginalControlPoints:OriginalControlPoints - 1;
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
			EndSegment.Append(GetSubdivisionBetweenGivenAndControlPoint(
				TargetSpline, EndShrink, BackTraverseIndex, true,
				MaxResampleDistance, false));
		}
		FTransform LastTransform = TargetSpline->GetTransformAtDistanceAlongSpline(
			(OriginalSplineLength - EndShrink), ESplineCoordinateSpace::Local, true);
		EndSegment.Emplace(LastTransform);

		for (int i = FrontTraverseIndex; i < BackTraverseIndex; ++i)
		{
			TArray<FVector> SubdivisionPoint;
			TargetSpline->ConvertSplineSegmentToPolyLine(i, ESplineCoordinateSpace::Local, MaxResampleDistance,
			                                             SubdivisionPoint);
			//每一段取左闭右开
			for (int j = 0; j < SubdivisionPoint.Num() - 1; ++j)
			{
				float DisToSubdivisionPoint = TargetSpline->GetDistanceAlongSplineAtLocation(
					SubdivisionPoint[j], ESplineCoordinateSpace::Local);
				ResamplePointsOnSpline.Emplace(
					TargetSpline->GetTransformAtDistanceAlongSpline(
						DisToSubdivisionPoint, ESplineCoordinateSpace::Local, true));
			}
		}
		//这个能集合两种情况FrontTraverseIndex!=BackTraverseIndex和FrontTraverseIndex==BackTraverseIndex
		ResamplePointsOnSpline.Emplace(
			TargetSpline->GetTransformAtSplinePoint(BackTraverseIndex, ESplineCoordinateSpace::Local, true));
		ResamplePointsOnSpline.Append(EndSegment);
	}
	OutResampledTransform = MoveTemp(ResamplePointsOnSpline);
	return true;
}

TArray<FTransform> URoadGeneratorSubsystem::GetSubdivisionBetweenGivenAndControlPoint(
	const USplineComponent* TargetSpline, float TargetLength, int32 NeighborIndex, bool bIsBackTraverse,
	float MaxResampleDistance, bool bIsClosedInterval)
{
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
	//如果进行了跳点，检查跳过点到下一个点之间的
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
			const float ThresholdValue = TargetLength + 0.25 * MaxResampleDistance;
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
		const float LengthToShrinkEnd = TargetSpline->GetSplineLength() - TargetLength;
		ResultTransforms.Emplace(
			TargetSpline->GetTransformAtDistanceAlongSpline(LengthToShrinkEnd, ESplineCoordinateSpace::Local, true));
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
