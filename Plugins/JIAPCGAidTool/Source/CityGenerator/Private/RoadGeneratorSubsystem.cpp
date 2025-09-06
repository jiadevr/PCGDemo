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
                                                        const ELaneType LaneTypeEnum)
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
	TArray<FTransform> SweepPath = ResampleSamplePoint(TargetSpline);
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

bool URoadGeneratorSubsystem::ResampleSamplePoint(const USplineComponent* TargetSpline,TArray<FTransform>& OutResampledTransform, 
                                                                double StartShrink, double EndShrink)
{
	//发现一个相似类型的函数，尝试一下这个函数的效果
	TArray<FTransform> ResamplePointsOnSpline{FTransform::Identity};

	if (nullptr != TargetSpline)
	{
		//直线和曲线有不同的差值策略
		//点属性管理的是后边一段，直线部分不应该插值；
		const int32 OriginalControlPoints = TargetSpline->GetNumberOfSplinePoints();
		ResamplePointsOnSpline.Reserve(OriginalControlPoints);
		//处理第一个点
		int32 StartPointIndex = 0;
		FTransform FirstTransform = TargetSpline->GetTransformAtSplinePoint(0, ESplineCoordinateSpace::Local, true);
		if (StartShrink > 0.0)
		{
			
			while (StartShrink > TargetSpline->GetDistanceAlongSplineAtSplinePoint(StartPointIndex)&&StartPointIndex < TargetSpline->GetNumberOfSplinePoints())
			{
				StartPointIndex++;
			}
			if (StartPointIndex >= TargetSpline->GetNumberOfSplinePoints())
			{
				UNotifyUtilities::ShowPopupMsgAtCorner("Shrink Value Longer Than SplineValue,InValid Spline");
				return false;
			}
			FirstTransform=TargetSpline->GetTransformAtDistanceAlongSpline(StartShrink,ESplineCoordinateSpace::Local,true);
		}
		//这里需要检查是直接结尾添加还是从头添加
		ResamplePointsOnSpline.Emplace(FirstTransform);
		//检查是否跳过点
		if (StartPointIndex > 0)
		{
			ESplinePointType::Type PreviousPointType = TargetSpline->GetSplinePointType(StartPointIndex-1);
			//如果进行了跳点，检查跳过点到下一个点之间的
			if (PreviousPointType!=ESplinePointType::Linear)
			{
				double RemainingLengthOfThisSegment
			}
		}

		ResamplePointsOnSpline[0] =
		for (int i = 1; i < OriginalControlPoints; ++i)
		{
			float SegmentLength = TargetSpline->GetDistanceAlongSplineAtSplinePoint(i) - TargetSpline->
				GetDistanceAlongSplineAtSplinePoint(i - 1);

			ESplinePointType::Type PointType = TargetSpline->GetSplinePointType(i);
			if (PointType == ESplinePointType::Linear)
			{
			}
			else
			{
				
			}
		}
		//后处理，对shou'wei

		double ResampleSplineLength = TargetSpline->GetSplineLength() - StartShrink - EndShrink;
		int32 ResamplePointCount = FMath::CeilToInt(ResampleSplineLength / CurveResampleLengthInCM);
		//double ActualResampleLengthInCM = ResampleSplineLength / ResamplePointCount;
		ResamplePointsOnSpline.SetNum(TargetSpline->IsClosedLoop() ? ResamplePointCount + 1 : ResamplePointCount);
		for (int32 i = 0; i < ResamplePointCount; ++i)
		{
			double DistanceFromSplineStart = FMath::Clamp(i * CurveResampleLengthInCM + StartShrink, StartShrink,
			                                              ResampleSplineLength + StartShrink);
			ResamplePointsOnSpline[i] = TargetSpline->GetTransformAtDistanceAlongSpline(
				DistanceFromSplineStart, ESplineCoordinateSpace::Local, true);
		}
		if (TargetSpline->IsClosedLoop())
		{
			FTransform ResampleStartPointTrans = TargetSpline->GetTransformAtDistanceAlongSpline(
				StartShrink, ESplineCoordinateSpace::Local, true);
			ResamplePointsOnSpline[ResamplePointCount] = ResampleStartPointTrans;
		}
		//@TODO:ActorTransform会造成影响
		/*if (nullptr!=TargetSpline->GetOwner())
		{
			const FTransform ActorTrans=TargetSpline->GetOwner()->GetActorTransform();
			for (auto& PointsOnSpline : ResamplePointsOnSpline)
			{
				PointsOnSpline.SetLocation(UKismetMathLibrary::TransformLocation(ActorTrans, PointsOnSpline.GetLocation()));
			}
		}*/
	}
	return MoveTemp(ResamplePointsOnSpline);
}

TArray<FVector> URoadGeneratorSubsystem::TestNativeSubdivisionFunction(const USplineComponent* TargetSpline)
{
	TArray<FVector> SubdivisionPoints;
	TargetSpline->ConvertSplineSegmentToPolyLine(0,ESplineCoordinateSpace::World,100.0f,SubdivisionPoints);
	return MoveTemp(SubdivisionPoints);
}

#pragma endregion GenerateRoad
