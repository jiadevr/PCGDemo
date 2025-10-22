// Fill out your copyright notice in the Description page of Project Settings.


#include "Road/RoadMeshGenerator.h"

#include "NotifyUtilities.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SplineComponent.h"
#include "GeometryScript/MeshMaterialFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "Kismet/KismetMathLibrary.h"
#include "Subsystems/EditorAssetSubsystem.h"

int32 URoadMeshGenerator::RoadGlobalIndex = -1;
// Sets default values for this component's properties
URoadMeshGenerator::URoadMeshGenerator()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	GlobalIndex = RoadGlobalIndex;
	RoadGlobalIndex++;
}

void URoadMeshGenerator::DrawDebugElemOnSweepPoint()
{
	AActor* OwnerActor = GetOwner();
	if (nullptr == OwnerActor)
	{
		return;
	}
	FTransform OwnerTransform = OwnerActor->GetActorTransform();
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
	int32 SweepPathLength = InRoadWithConnect.ContinuousSegmentsTrans.Num();
	SweepPathLength += InRoadWithConnect.bHasHeadConnection ? 1 : 0;
	SweepPathLength += InRoadWithConnect.bHasTailConnection ? 1 : 0;
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
	bIsLocalSpace = false;
	StartToIntersectionIndex = InRoadWithConnect.FromIntersectionIndex;
	EndToIntersectionIndex = InRoadWithConnect.ToIntersectionIndex;
}

bool URoadMeshGenerator::GenerateMesh()
{
	AActor* Owner = GetOwner();
	ensureAlwaysMsgf(nullptr!=Owner, TEXT("Component Has No Owner"));
	if (SweepPointsTrans.IsEmpty())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner(
			FString::Printf(
				TEXT("[ERROR]%s Create Intersection Failed,Spline Data Is Empty"), *Owner->GetActorLabel()));
		return false;
	}
	if (!MeshComponent.IsValid())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner(
			FString::Printf(
				TEXT("[ERROR]%s Create Intersection Failed,Null MeshComp Found!"), *Owner->GetActorLabel()));
		return false;
	}
	UDynamicMeshComponent* MeshComponentPtr = MeshComponent.Pin().Get();

	TArray<FVector2D> SweepShape = RoadInfo.CrossSectionCoord;
	if (SweepShape.IsEmpty())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner(
			FString::Printf(
				TEXT("[ERROR]%s Create Intersection Failed,CrossSection Shape Is Not Defined"),
				*Owner->GetActorLabel()));
		return false;
	}
	if (!bIsLocalSpace)
	{
		ConvertPointToLocalSpace(Owner->GetTransform());
	}
	FGeometryScriptPrimitiveOptions GeometryScriptOptions;
	FTransform SweepMeshTrans = FTransform::Identity;
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(MeshComponentPtr->GetDynamicMesh(),
	                                                                  GeometryScriptOptions, SweepMeshTrans, SweepShape,
	                                                                  SweepPointsTrans);
	FGeometryScriptSplitNormalsOptions SplitOptions;
	FGeometryScriptCalculateNormalsOptions CalculateOptions;
	UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(MeshComponent->GetDynamicMesh(), SplitOptions,
	                                                                 CalculateOptions);
	if (nullptr == Material)
	{
		InitialMaterials();
	}
	RefreshMatsOnDynamicMeshComp();
	return true;
}

void URoadMeshGenerator::SetMeshComponent(class UDynamicMeshComponent* InMeshComponent)
{
	if (InMeshComponent != nullptr)
	{
		MeshComponent = TWeakObjectPtr<UDynamicMeshComponent>(InMeshComponent);
	}
}

TArray<FVector> URoadMeshGenerator::GetRoadEdgePoints(bool bForwardOrderDir)
{
	TArray<FVector> Results;
	if (SweepPointsTrans.IsEmpty())
	{
		return Results;
	}
	Results.SetNum(SweepPointsTrans.Num());
	for (int32 i = 0; i < SweepPointsTrans.Num(); ++i)
	{
		int32 TargetIndex = bForwardOrderDir ? i : SweepPointsTrans.Num() - 1 - i;
		FVector CenterLocation = UKismetMathLibrary::TransformLocation(GetOwner()->GetTransform(),
		                                                               SweepPointsTrans[i].GetLocation());
		FRotator CenterRotation = UKismetMathLibrary::TransformRotation(GetOwner()->GetTransform(),
		                                                                SweepPointsTrans[i].GetRotation().Rotator());
		//在路口左转，取道路左边线
		FVector CenterToEdge = RoadInfo.CrossSectionCoord[0].X * CenterRotation.Vector().RotateAngleAxis(
			bForwardOrderDir ? -90.0 : 90.0, FVector::UpVector);
		Results[TargetIndex] = CenterLocation + CenterToEdge;
	}
	return Results;
}

FInterpCurveVector URoadMeshGenerator::GetSplineControlPointsInRoadRange(bool bForwardOrderDir,
                                                                         ECoordOffsetType OffsetType,
                                                                         float CustomOffsetOnLeft)
{
	FInterpCurveVector Result;
	//TArray<FVector> Results;
	if (!ReferenceSpline.IsValid())
	{
		return Result;
	}
	USplineComponent* OwnerSpline = ReferenceSpline.Pin().Get();
	AActor* OwnerActor = GetOwner();
	if (OwnerActor == nullptr) { return Result; }
	const float DirectionScalar = bForwardOrderDir ? -1.0f : 1.0f;

	FVector RoadStartLocation = SweepPointsTrans[0].GetLocation();
	RoadStartLocation = UKismetMathLibrary::TransformLocation(OwnerActor->GetTransform(), RoadStartLocation);
	//下面这个函数有问题，输入数字大的时候使用Local可能会输出(0,0,0)
	float StartAsDist = OwnerSpline->GetDistanceAlongSplineAtLocation(RoadStartLocation, ESplineCoordinateSpace::World);
	float StartAsInputKey = OwnerSpline->GetInputKeyValueAtDistanceAlongSpline(StartAsDist);
	//FVector StartTangent = OwnerSpline->GetTangentAtDistanceAlongSpline(StartAsDist, ESplineCoordinateSpace::World);
	//处理偏移问题
	GetWSPointFromRoadCenterWithOffset(RoadStartLocation, StartAsDist, OwnerSpline, bForwardOrderDir,
	                                   OffsetType, CustomOffsetOnLeft);
	FInterpCurvePoint<FVector> StartPoint(bForwardOrderDir ? 0.0f : 1.0f, RoadStartLocation, FVector::ZeroVector,
	                                      FVector::ZeroVector, CIM_Constant);

	FVector RoadEndLocation = SweepPointsTrans.Last().GetLocation();
	RoadEndLocation = UKismetMathLibrary::TransformLocation(OwnerActor->GetTransform(), RoadEndLocation);
	//下面这个函数有问题，输入数字大的时候使用Local可能会输出(0,0,0)
	float EndAsDist = OwnerSpline->GetDistanceAlongSplineAtLocation(RoadEndLocation, ESplineCoordinateSpace::World);
	float EndAsInputKey = OwnerSpline->GetInputKeyValueAtDistanceAlongSpline(EndAsDist);
	//FVector EndTangent = OwnerSpline->GetTangentAtDistanceAlongSpline(EndAsDist, ESplineCoordinateSpace::World);
	//处理偏移
	GetWSPointFromRoadCenterWithOffset(RoadEndLocation, EndAsDist, OwnerSpline, bForwardOrderDir,
	                                   OffsetType, CustomOffsetOnLeft);
	FInterpCurvePoint<FVector> EndPoint(bForwardOrderDir ? 1.0f : 0.0f, RoadEndLocation, FVector::ZeroVector,
	                                    FVector::ZeroVector,
	                                    CIM_Constant);

	//起点到终点跨过的ControlPoints个数，需要特别考虑Loop类型End<Start
	int32 ControlPointNumInRange = FMath::FloorToInt(EndAsInputKey) - FMath::FloorToInt(StartAsInputKey);
	int32 NumControlPoints = OwnerSpline->GetNumberOfSplinePoints();
	bool bInLoopArea = false;
	//Loop类型Start从较大值绕回到Start
	if (ControlPointNumInRange < 0)
	{
		ensure(OwnerSpline->IsClosedLoop());
		ControlPointNumInRange = NumControlPoints - FMath::CeilToInt(StartAsInputKey) +
			FMath::CeilToInt(EndAsInputKey);
		bInLoopArea = true;
	}
	Result.Points.Reserve(2 + ControlPointNumInRange);
	//Results.Reserve(2 + ControlPointNumInRange);
	const FInterpCurvePoint<FVector>& FirstElem = bForwardOrderDir ? StartPoint : EndPoint;
	Result.Points.Add(FirstElem);
	//Results.Emplace(bForwardOrderDir ? RoadStartLocation : RoadEndLocation);
	//const FVector& LastElem = bForwardOrderDir ? RoadEndLocation : RoadStartLocation;
	const FInterpCurvePoint<FVector>& LastElem = bForwardOrderDir ? EndPoint : StartPoint;
	//是结尾到开头连接的部分
	if (ControlPointNumInRange > 0)
	{
		for (int32 i = 1; i <= ControlPointNumInRange; i++)
		{
			int32 ResultIndex = bForwardOrderDir
				                    ? (FMath::FloorToInt(StartAsInputKey) + i) % (NumControlPoints)
				                    : (FMath::CeilToInt(EndAsInputKey) - i + NumControlPoints) % (NumControlPoints);

			float InputKeyAsFloat = static_cast<float>(ResultIndex);
			FVector ControlPointLocWS = OwnerSpline->GetLocationAtSplineInputKey(InputKeyAsFloat,
				ESplineCoordinateSpace::World);
			//只要调整了Tangent控制柄就会变成CustomTangent,对应产生Rotation
			FVector ControlPointArriveTangentWS = (bForwardOrderDir ? 1.0 : -1.0) * OwnerSpline->
				GetArriveTangentAtSplinePoint(InputKeyAsFloat, ESplineCoordinateSpace::World);
			FVector ControlPointLeaveTangentWS = (bForwardOrderDir ? 1.0 : -1.0) * OwnerSpline->
				GetLeaveTangentAtSplinePoint(InputKeyAsFloat, ESplineCoordinateSpace::World);

			ESplinePointType::Type PointType = OwnerSpline->GetSplinePointType(ResultIndex);
			//对于获取边缘的情况
			GetWSPointFromRoadCenterWithOffset(ControlPointLocWS, ResultIndex, OwnerSpline, bForwardOrderDir,
			                                   OffsetType, CustomOffsetOnLeft);
			float TempTime = i / (2.0f + ControlPointNumInRange);
			//对于Curve类型两个Tangent值相同
			FInterpCurvePoint<FVector> MidPoint(TempTime, ControlPointLocWS, ControlPointArriveTangentWS,
			                                    ControlPointLeaveTangentWS,
			                                    ConvertSplinePointTypeToInterpCurveMode(PointType));
			/*if (CIM_CurveAuto == MidPoint.InterpMode||CIM_CurveUser == MidPoint.InterpMode)
			{
				MidPoint.InterpMode = CIM_CurveAutoClamped;
			}*/

			if (i == 1 || i == ControlPointNumInRange)
			{
				//两者均已经转化为世界坐标
				double DistanceToNeighbour = FVector::DistSquared(ControlPointLocWS,
				                                                  i == 1 ? FirstElem.OutVal : LastElem.OutVal);
				if (DistanceToNeighbour <= 10000)
				{
					continue;
				}
			}
			if (bInLoopArea && ResultIndex == OwnerSpline->GetNumberOfSplinePoints() - 1)
			{
				//对最后一个点到第一个点在后边插入一个点，避免CloseLoop时强制设置Tangent使得Spline走形
				float AppendPointDis = OwnerSpline->GetSplineLength() - 100.0f;
				FVector AppendPointLocWS = OwnerSpline->GetLocationAtDistanceAlongSpline(
					AppendPointDis, ESplineCoordinateSpace::World);
				DrawDebugSphere(GetWorld(), AppendPointLocWS, 100.0f, 10, FColor::Red, true, -1, 0, 5.0f);
				UE_LOG(LogTemp, Display, TEXT("Add Location At %s Distance %f"), *AppendPointLocWS.ToString(),
				       AppendPointDis);
				GetWSPointFromRoadCenterWithOffset(AppendPointLocWS, AppendPointDis, OwnerSpline, bForwardOrderDir,
				                                   OffsetType, CustomOffsetOnLeft);
				FVector AppendPointTangentWS = (bForwardOrderDir ? 1.0 : -1.0) * OwnerSpline->
					GetTangentAtDistanceAlongSpline(AppendPointDis, ESplineCoordinateSpace::World);
				//这个值行不行待测试
				TempTime += 0.25f / (2.0f + ControlPointNumInRange);
				FInterpCurvePoint<FVector> AppendPoint(TempTime, AppendPointLocWS, AppendPointTangentWS,
				                                       AppendPointTangentWS, CIM_Constant);
				if (bForwardOrderDir)
				{
					Result.Points.Add(MidPoint);
				}
				Result.Points.Add(AppendPoint);
				continue;
			}
			Result.Points.Add(MidPoint);
		}
	}
	//已经是世界空间了
	//Results.Emplace(LastElem);
	Result.Points.Emplace(LastElem);
	return Result;
}

void URoadMeshGenerator::GetConnectionOrderOfIntersection(int32& OutLocFromIndex, int32& OutLocEndIndex) const
{
	OutLocFromIndex = StartToIntersectionIndex;
	OutLocEndIndex = EndToIntersectionIndex;
}

void URoadMeshGenerator::ConvertPointToLocalSpace(const FTransform& InActorTransform)
{
	TArray<FTransform> Results;
	for (auto& SinglePathPointTrans : SweepPointsTrans)
	{
		FVector LocationInLocalSpace = UKismetMathLibrary::InverseTransformLocation(
			InActorTransform, SinglePathPointTrans.GetLocation());
		SinglePathPointTrans.SetLocation(LocationInLocalSpace);
		FRotator RotatorInLocalSpace = UKismetMathLibrary::InverseTransformRotation(
			InActorTransform, SinglePathPointTrans.GetRotation().Rotator());
		SinglePathPointTrans.SetRotation(RotatorInLocalSpace.Quaternion());
	}
	bIsLocalSpace = true;
}

void URoadMeshGenerator::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property)
	{
		FName PropertyName = PropertyChangedEvent.Property->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(URoadMeshGenerator, Material))
		{
			RefreshMatsOnDynamicMeshComp();
		}
	}
}

void URoadMeshGenerator::RefreshMatsOnDynamicMeshComp()
{
	if (nullptr != Material && MeshComponent.IsValid())
	{
		TArray<UMaterialInterface*> Materials;
		Materials.Emplace(Material);
		MeshComponent->ConfigureMaterialSet(Materials);
	}
}

void URoadMeshGenerator::InitialMaterials()
{
	FString TargetPath;
	UEditorAssetSubsystem* AssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	if (!AssetSubsystem)
	{
		return;
	}
	if (AssetSubsystem->DoesAssetExist(MaterialPath))
	{
		TargetPath = MaterialPath;
	}
	else
	{
		if (AssetSubsystem->DoesAssetExist(BackupMaterialPath))
		{
			TargetPath = BackupMaterialPath;
		}
	}
	if (TargetPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("No Valid Material"));
		return;
	}
	UObject* TargetMatAsset = AssetSubsystem->LoadAsset(TargetPath);
	if (TargetMatAsset)
	{
		Material = Cast<UMaterialInterface>(TargetMatAsset);
	}
}

void URoadMeshGenerator::GetWSPointFromRoadCenterWithOffset(FVector& PointOnRoadCenter, float DistanceFromStart,
                                                            const USplineComponent* OwnerSpline, bool bForwardOrderDir,
                                                            ECoordOffsetType OffsetType, float CustomOffsetOnLeft)
{
	if (OffsetType == ECoordOffsetType::LEFTEDGE || OffsetType == ECoordOffsetType::CUSTOM)
	{
		const float DirectionScalar = bForwardOrderDir ? -1.0 : 1.0;
		FVector EndRightVector = OwnerSpline->GetRightVectorAtDistanceAlongSpline(
			DistanceFromStart, ESplineCoordinateSpace::World);
		float OffsetValue = CustomOffsetOnLeft;
		if (OffsetType == ECoordOffsetType::LEFTEDGE)
		{
			OffsetValue = RoadInfo.CrossSectionCoord[0].X;
		}
		PointOnRoadCenter += OffsetValue * EndRightVector * DirectionScalar;
	}
}

void URoadMeshGenerator::GetWSPointFromRoadCenterWithOffset(FVector& PointOnRoadCenter, int32 ControlPointIndex,
                                                            const USplineComponent* OwnerSpline, bool bForwardOrderDir,
                                                            ECoordOffsetType OffsetType, float CustomOffsetOnLeft)
{
	if (OffsetType == ECoordOffsetType::LEFTEDGE || OffsetType == ECoordOffsetType::CUSTOM)
	{
		const float DirectionScalar = bForwardOrderDir ? -1.0 : 1.0;
		FVector EndRightVector = OwnerSpline->GetRightVectorAtSplinePoint(
			ControlPointIndex, ESplineCoordinateSpace::World);
		float OffsetValue = CustomOffsetOnLeft;
		if (OffsetType == ECoordOffsetType::LEFTEDGE)
		{
			OffsetValue = RoadInfo.CrossSectionCoord[0].X;
		}
		PointOnRoadCenter += OffsetValue * EndRightVector * DirectionScalar;
	}
}
