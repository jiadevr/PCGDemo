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
#include "Road/RoadDataComp.h"

#pragma region GenerateRoad

void URoadGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	//双向四车道是2*7.5米，双向六车道是2*11.25米
	RoadPresetMap.Emplace(ELaneType::SingleWay, FLaneMeshInfo(400.0f, 20.0f));
	RoadPresetMap.Emplace(ELaneType::TwoLaneTwoWay, FLaneMeshInfo(700.0f, 20.0f));
}


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
	TObjectPtr<URoadDataComp> RoadDataComp = Cast<URoadDataComp>(
		UEditorComponentUtilities::AddComponentInEditor(MeshActor, URoadDataComp::StaticClass()));
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

void URoadGeneratorSubsystem::GenerateRoadInterSection(TArray<USplineComponent*> TargetSplines, float RoadWidth)
{
	if (TargetSplines.Num() < 2)
	{
		return;
	}
	//测试内容
	FVector L0P0 = TargetSplines[0]->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
	FVector L0P1 = TargetSplines[0]->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::World);
	FVector L1P0 = TargetSplines[1]->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
	FVector L1P1 = TargetSplines[1]->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::World);
	//交点计算
	TArray<FVector2D> Intersections2D;
	if (!Get2DIntersection(TargetSplines, Intersections2D))
	{
		return;
	}
	FlushPersistentDebugLines(GEditor->GetWorld());
	for (auto Intersection : Intersections2D)
	{
		FVector IntersectionPoint = FVector(Intersection, 0.0);
		DrawDebugSphere(TargetSplines[0]->GetWorld(), IntersectionPoint, 15.0f, 12, FColor::Purple, false, 20.0f);
	}
	
	/*FVector L0P0O0 = L0P0 + TargetSplines[0]->GetRightVectorAtSplinePoint(0, ESplineCoordinateSpace::World) * RoadWidth
		* 0.5f;
	FVector L0P0O1 = L0P0 + TargetSplines[0]->GetRightVectorAtSplinePoint(0, ESplineCoordinateSpace::World) * RoadWidth
		* -0.5f;
	FVector L0P1O0 = L0P1 + TargetSplines[0]->GetRightVectorAtSplinePoint(1, ESplineCoordinateSpace::World) * RoadWidth
		* 0.5f;
	FVector L0P1O1 = L0P1 + TargetSplines[0]->GetRightVectorAtSplinePoint(1, ESplineCoordinateSpace::World) * RoadWidth
		* -0.5f;
	FVector L1P0O0 = L1P0 + TargetSplines[1]->GetRightVectorAtSplinePoint(0, ESplineCoordinateSpace::World) * RoadWidth
		* 0.5f;
	FVector L1P0O1 = L1P0 + TargetSplines[1]->GetRightVectorAtSplinePoint(0, ESplineCoordinateSpace::World) * RoadWidth
		* -0.5f;
	FVector L1P1O0 = L1P1 + TargetSplines[1]->GetRightVectorAtSplinePoint(1, ESplineCoordinateSpace::World) * RoadWidth
		* 0.5f;
	FVector L1P1O1 = L1P1 + TargetSplines[1]->GetRightVectorAtSplinePoint(1, ESplineCoordinateSpace::World) * RoadWidth
		* -0.5f;
	TArray<FVector> PointsAroundIntersection{L0P0O0, L0P0O1, L0P1O0, L0P1O1, L1P0O0, L1P0O1, L1P1O0, L1P1O1};
	//对点进行顺时针排序
	PointsAroundIntersection.Sort([&IntersectionPoint](const FVector& A, const FVector& B)
	{
		FVector ProjectedA = FVector::VectorPlaneProject((A - IntersectionPoint), FVector::UnitZ());
		FVector2D RelA{ProjectedA.X, ProjectedA.Y};
		FVector ProjectedB = FVector::VectorPlaneProject((B - IntersectionPoint), FVector::UnitZ());
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
	const int32 PointCount = PointsAroundIntersection.Num();
	for (int32 i = 0; i < PointsAroundIntersection.Num(); ++i)
	{
		FColor DebugColor = FColor(255 * i / PointCount, 0, 0, 255);
		DrawDebugSphere(TargetSplines[0]->GetWorld(), PointsAroundIntersection[i], 20.0f, 12, DebugColor, false,
		                20.0f);
	}*/
}

bool URoadGeneratorSubsystem::Get2DIntersection(TArray<USplineComponent*> TargetSplines,
                                                TArray<FVector2D>& IntersectionsIn2DSpace)
{
	TArray<FVector2D> Results;
	if (TargetSplines.Num() != 2)
	{
		return false;
	}
	struct FSplineBezierSegment
	{
	public:
		FVector P0, P1, P2, P3;

		FVector GetLocation(float InputKeyFromPreControlPoint) const
		{
			return FMath::CubicInterp(P0, (P1 - P0) * 3.0f, P3, (P3 - P2) * 3.0f, InputKeyFromPreControlPoint);
		}

		FVector GetDerivative(float InputKeyFromPreControlPoint) const
		{
			float InputKeyFromNextControlPoint = 1.0f - InputKeyFromPreControlPoint;
			return 3.0f * InputKeyFromNextControlPoint * InputKeyFromNextControlPoint * (P1 - P0) +
				6.0f * InputKeyFromNextControlPoint * InputKeyFromPreControlPoint * (P2 - P1) +
				3.0f * InputKeyFromPreControlPoint * InputKeyFromPreControlPoint * (P3 - P2);
		}
	};

	auto Newton2DSolver = [](const FSplineBezierSegment& A, const FSplineBezierSegment& B, FVector2D Seed,
	                         FVector2D& Intersection, float Tolerance, int32 MaxIteration)-> bool
	{
		FVector2D ApproximateResult = Seed;
		for (int32 i = 0; i < MaxIteration; ++i)
		{
			FVector LocationInA = A.GetLocation(ApproximateResult.X);
			FVector LocationInB = B.GetLocation(ApproximateResult.Y);
			FVector2D VectorAToB{(LocationInA - LocationInB).X, (LocationInA - LocationInB).Y};
			if (VectorAToB.SizeSquared() < Tolerance * Tolerance)
			{
				Intersection = ApproximateResult;
				return true;
			}
			FVector DerivativeInA = A.GetDerivative(ApproximateResult.X);
			FVector DerivativeInB = B.GetDerivative(ApproximateResult.Y);
			//UE不支持直接逆矩阵与向量相乘，所以构造一个矩阵意义不大
			FMatrix2x2d Jacobian{DerivativeInA.X, -DerivativeInB.X, DerivativeInA.Y, -DerivativeInB.Y};
			float Determinant = Jacobian.Determinant();
			//雅可比矩阵行列式绝对值表示两条曲线速度向量形成的平行四变相面颊越大越好，值小会造成牛顿步爆炸；或者理解成矩阵可逆
			if (FMath::Abs(Determinant) < 1e-6f)
			{
				return false;
			}
			float InvDet = 1.0f / Determinant;
			FVector2D Delta{
				(-DerivativeInB.Y * VectorAToB.X - (-DerivativeInB.X) * VectorAToB.Y) * InvDet,
				(-DerivativeInA.Y * VectorAToB.X + DerivativeInA.X * VectorAToB.Y) * InvDet
			};
			ApproximateResult -= Delta;
			//Clamp在[0,1]
			ApproximateResult.X = FMath::Clamp(ApproximateResult.X, 0.0f, 1.0f);
			ApproximateResult.Y = FMath::Clamp(ApproximateResult.Y, 0.0f, 1.0f);
		}
		return false;
	};

	auto GetSegment = [](const USplineComponent* S, int32 Index)-> FSplineBezierSegment
	{
		FSplineBezierSegment Sg;
		Sg.P0 = S->GetLocationAtSplineInputKey(Index, ESplineCoordinateSpace::World);
		Sg.P3 = S->GetLocationAtSplineInputKey(Index + 1, ESplineCoordinateSpace::World);
		// 切线控制点
		Sg.P1 = Sg.P0 + S->GetTangentAtSplineInputKey(Index, ESplineCoordinateSpace::World) / 3.f;
		Sg.P2 = Sg.P3 - S->GetTangentAtSplineInputKey(Index + 1, ESplineCoordinateSpace::World) / 3.f;
		return Sg;
	};

	//Newton-Raphson求三次贝塞尔线段交点
	const int32 NumOfSegmentA = TargetSplines[0]->GetNumberOfSplineSegments();
	const int32 NumOfSegmentB = TargetSplines[1]->GetNumberOfSplineSegments();
	for (int32 SegIndexA = 0; SegIndexA < NumOfSegmentA; ++SegIndexA)
	{
		FSplineBezierSegment CurrentSegmentA = GetSegment(TargetSplines[0], SegIndexA);
		for (int32 SegIndexB = 0; SegIndexB < NumOfSegmentB; ++SegIndexB)
		{
			FSplineBezierSegment CurrentSegmentB = GetSegment(TargetSplines[1], SegIndexB);
			for (float SeedT = 0.05f; SeedT < 1.f; SeedT += 0.013f)
			{
				for (float SeedS = 0.05f; SeedS < 1.f; SeedS +=  0.017f)
				{
					FVector2D TS;
					if (Newton2DSolver(CurrentSegmentA, CurrentSegmentB, FVector2D(SeedT, SeedS),TS, 0.01f,100))
					{
						FVector2D P {CurrentSegmentA.GetLocation(TS.X).X,CurrentSegmentA.GetLocation(TS.X).Y};
						// 去重
						bool bNew = true;
						for (const auto& Ex : IntersectionsIn2DSpace)
							if (FVector2D::DistSquared(Ex, P) < 25.f)
							{
								bNew = false;
								break;
							}
						if (bNew)
							IntersectionsIn2DSpace.Emplace(P);
					}
				}
			}
		}
	}


/*//直线段求交简化情况验证
FVector L0P0 = TargetSplines[0]->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
FVector L0P1 = TargetSplines[0]->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::World);
FVector L1P0 = TargetSplines[1]->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
FVector L1P1 = TargetSplines[1]->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::World);
//消元法
double Denominator = (L0P1.X - L0P0.X) * (L1P1.Y - L1P0.Y) - (L0P1.Y - L0P0.Y) * (L1P1.X - L1P0.X);
if (Denominator != 0)
{
	double IntersectionT = ((L1P0.X - L0P0.X) * (L1P1.Y - L1P0.Y) - (L1P0.Y - L0P0.Y) * (L1P1.X - L1P0.X)) /
		Denominator;
	double IntersectionS = ((L1P0.X - L0P0.X) * (L0P1.Y - L0P0.Y) - (L1P1.Y - L0P0.Y) * (L0P1.X - L0P0.X)) /
		Denominator;
	if (0.0 <= IntersectionT && IntersectionT <= 1.0 && 0.0 <= IntersectionS && IntersectionS <= 1.0)
	{
		FVector2D Intersection{
			L0P0.X + IntersectionT * (L0P1.X - L0P0.X), L0P0.Y + IntersectionT * (L0P1.Y - L0P0.Y)
		};
		Results.Emplace(Intersection);
		IntersectionsIn2DSpace = MoveTemp(Results);
		return true;
	}
}*/
return !IntersectionsIn2DSpace.IsEmpty();
}

FVector URoadGeneratorSubsystem::CalculateTangentPoint(const FVector& Intersection, const FVector& EdgePoint)
{
	FVector Dir = UKismetMathLibrary::GetDirectionUnitVector(EdgePoint, Intersection);
	FVector Tangent = FVector::Dist(EdgePoint, Intersection) * 2 * Dir;
	return Tangent;
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
