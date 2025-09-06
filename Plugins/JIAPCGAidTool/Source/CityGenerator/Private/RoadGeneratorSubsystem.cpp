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
	TArray<FVector2D> SweepShape=RoadPresetMap[LaneTypeEnum].CrossSectionCoord;
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(DynamicMesh, GeometryScriptOptions,
	                                                                  SweepMeshTrans, SweepShape, SweepPath);
	UGeometryScriptLibrary_MeshNormalsFunctions::AutoRepairNormals(DynamicMesh);
	FGeometryScriptSplitNormalsOptions SplitOptions;
	FGeometryScriptCalculateNormalsOptions CalculateOptions;
	UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(DynamicMesh, SplitOptions, CalculateOptions);
}

TArray<FTransform> URoadGeneratorSubsystem::ResampleSamplePoint(const USplineComponent* TargetSpline,
                                                                double StartShrink, double EndShrink)
{
	TArray<FTransform> ResamplePointsOnSpline{FTransform::Identity};
	if (nullptr != TargetSpline)
	{
		//直线和曲线有不同的差值策略
		//点属性管理的是后边一段
		const int32 OriginControlPoints = TargetSpline->GetNumberOfSplinePoints();
		for (int i = 0; i < OriginControlPoints; ++i)
		{
			//ESplinePointType::Type PointType=TargetSpline->GetSplinePointType()
		}

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

#pragma endregion GenerateRoad
