// Fill out your copyright notice in the Description page of Project Settings.


#include "Road/RoadGeometryUtilities.h"
#include "Components/SplineComponent.h"

bool URoadGeometryUtilities::Get2DIntersection(const FVector2D& InSegmentAStart, const FVector2D& InSegmentAEnd,
                                               const FVector2D& InSegmentBStart, const FVector2D& InSegmentBEnd,
                                               FVector2D& OutIntersection)
{
	//快速排斥测试
	if (FMath::Max(InSegmentAStart.X, InSegmentAEnd.X) < FMath::Min(InSegmentBStart.X, InSegmentBEnd.X) ||
		FMath::Max(InSegmentBStart.X, InSegmentBEnd.X) < FMath::Min(InSegmentAStart.X, InSegmentAEnd.X) ||
		FMath::Max(InSegmentAStart.Y, InSegmentAEnd.Y) < FMath::Min(InSegmentBStart.Y, InSegmentBEnd.Y) ||
		FMath::Max(InSegmentBStart.Y, InSegmentBEnd.Y) < FMath::Min(InSegmentAStart.Y, InSegmentAEnd.Y))
	{
		return false;
	}

	// 计算线段AB和CD的参数方程交点
	FVector2D VectorA = (InSegmentAEnd - InSegmentAStart);
	FVector2D VectorB = (InSegmentBEnd - InSegmentBStart);
	FVector2D VectorABStart = InSegmentBStart - InSegmentAStart;
	//叉积获得AB逆时针夹角的Sin值，Sin值为0时两向量平行或共线
	float Denominator = FVector2D::CrossProduct(VectorA, VectorB);
	if (FMath::IsNearlyZero(Denominator))
	{
		return false;
	}
	//直线参数方程
	//(X0,Y0)=AStart+t*VectorA=BStart+S*VectorB
	//使用三角形相似计算比值
	float t = FVector2D::CrossProduct(VectorABStart, VectorB) / Denominator;
	float s = FVector2D::CrossProduct(VectorABStart, VectorA) / Denominator;

	if (t >= 0.0f && t <= 1.0f && s >= 0.0f && s <= 1.0f)
	{
		// 计算交点
		OutIntersection = InSegmentAStart + t * VectorA;
		return true;
	}

	return false;
}

bool URoadGeometryUtilities::Get2DIntersection(USplineComponent* TargetSplineA, USplineComponent* TargetSplineB,
                                               TArray<FVector2D>& IntersectionsIn2DSpace)
{
	TArray<FVector2D> Results;

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
	const int32 NumOfSegmentA = TargetSplineA->GetNumberOfSplineSegments();
	const int32 NumOfSegmentB = TargetSplineB->GetNumberOfSplineSegments();
	for (int32 SegIndexA = 0; SegIndexA < NumOfSegmentA; ++SegIndexA)
	{
		FSplineBezierSegment CurrentSegmentA = GetSegment(TargetSplineA, SegIndexA);
		for (int32 SegIndexB = 0; SegIndexB < NumOfSegmentB; ++SegIndexB)
		{
			FSplineBezierSegment CurrentSegmentB = GetSegment(TargetSplineB, SegIndexB);
			for (float SeedT = 0.05f; SeedT < 1.f; SeedT += 0.013f)
			{
				for (float SeedS = 0.05f; SeedS < 1.f; SeedS += 0.017f)
				{
					FVector2D TS;
					if (Newton2DSolver(CurrentSegmentA, CurrentSegmentB, FVector2D(SeedT, SeedS), TS, 0.01f, 100))
					{
						FVector2D P{CurrentSegmentA.GetLocation(TS.X).X, CurrentSegmentA.GetLocation(TS.X).Y};
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
	return !IntersectionsIn2DSpace.IsEmpty();
}

void URoadGeometryUtilities::SortPointClockwise(const FVector2D& Center, TArray<FVector2D>& ArrayToSort)
{
	ArrayToSort.Sort([&Center](const FVector2D& A, const FVector2D& B)
	{
		FVector2D ADir = A - Center;
		FVector2D BDir = B - Center;
		float AngleA = FMath::Atan2(ADir.Y, ADir.X);
		float AngleB = FMath::Atan2(BDir.Y, BDir.X);

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
			return ADir.SizeSquared() < BDir.SizeSquared();
		}
	});
}

double URoadGeometryUtilities::GetAreaOfSortedPoints(const TArray<FVector2D>& SortedVertex)
{
	double Area = 0.f;
	int32 VertexNum = SortedVertex.Num();
	for (int i = 0; i < VertexNum; ++i)
	{
		FVector2D VertexA = SortedVertex[i];
		FVector2D VertexB = SortedVertex[(i + 1) % VertexNum];
		Area += VertexA.X * VertexB.Y - VertexA.Y * VertexB.X;
	}
	return FMath::Abs(Area);
}

void URoadGeometryUtilities::SimplifySplinePointsInline(TArray<FVector>& SplinePoints, bool bIgnoreZ,
                                                        const float DisThreshold, const float AngleThreshold)
{
	const int32 OriginalNum = SplinePoints.Num();
	if (SplinePoints.Num() < 3)
	{
		return;
	}
	//角度计算中使用了Normalize，需要计算开方、除法，计算成本比较高，先算距离剔除，后面还可以缓存距离
	//剔除圆弧处细分点
	const double LengthThresholdSquared = DisThreshold * DisThreshold;
	TArray<FVector> PointsAfterDisSimplification;
	PointsAfterDisSimplification.Reserve(OriginalNum);
	//双指针查找圆弧细分段
	int32 LeftIndex = 0;
	while (LeftIndex < OriginalNum)
	{
		int32 RightIndex = LeftIndex;
		while (RightIndex < OriginalNum - 1)
		{
			const double SegmentLengthSquared = bIgnoreZ
				                                    ? FVector::DistSquaredXY(
					                                    SplinePoints[RightIndex], SplinePoints[RightIndex + 1])
				                                    : FVector::DistSquared(
					                                    SplinePoints[RightIndex], SplinePoints[RightIndex + 1]);
			if (SegmentLengthSquared > LengthThresholdSquared)
			{
				break;
			}
			RightIndex++;
		}
		// 对找到的圆弧细分段用首尾点简化
		if (RightIndex > LeftIndex)
		{
			PointsAfterDisSimplification.Add(SplinePoints[LeftIndex]);
			LeftIndex = RightIndex;
			continue;
		}
		// 单个长线段
		PointsAfterDisSimplification.Add(SplinePoints[LeftIndex]);
		LeftIndex++;
	}
	// 添加最后一个点
	if (PointsAfterDisSimplification.Last() != SplinePoints.Last())
	{
		PointsAfterDisSimplification.Add(SplinePoints.Last());
	}
	// 检查简化结果，如果删的太狠就直接返回
	if (PointsAfterDisSimplification.Num() < 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("DISTANCE SIMPLIFICATION RESULTED IN TOO FEW POINTS, ABANDON ANGLE FILTER"));
		SplinePoints = MoveTemp(PointsAfterDisSimplification);
		return;
	}
	//0.044对应sin(2.5),0.087对应sin(5.0)
	const double ParallelThreshold = FMath::Sin(FMath::DegreesToRadians(AngleThreshold));
	TArray<FVector> PointsAfterAngleSimplification;
	PointsAfterAngleSimplification.Reserve(PointsAfterDisSimplification.Num());
	PointsAfterAngleSimplification.Add(PointsAfterDisSimplification[0]);
	for (int32 i = 1; i < PointsAfterDisSimplification.Num() - 1; ++i)
	{
		if (!IsParallel(PointsAfterAngleSimplification.Last(), PointsAfterDisSimplification[i],
		                PointsAfterDisSimplification[i], PointsAfterDisSimplification[i + 1],
		                bIgnoreZ, ParallelThreshold))
		{
			PointsAfterAngleSimplification.Add(PointsAfterDisSimplification[i]);
		}
	}
	PointsAfterAngleSimplification.Add(PointsAfterDisSimplification.Last());
	//检查最后一个段方向是否和第一段重合
	if (PointsAfterAngleSimplification.Num() > 3 && IsParallel(PointsAfterAngleSimplification.Last(),
	                                                           PointsAfterDisSimplification[0],
	                                                           PointsAfterDisSimplification[0],
	                                                           PointsAfterDisSimplification[1], bIgnoreZ,
	                                                           ParallelThreshold))
	{
		PointsAfterAngleSimplification.RemoveAtSwap(0, 1, EAllowShrinking::Default);
	}
	SplinePoints = MoveTemp(PointsAfterAngleSimplification);
}

void URoadGeometryUtilities::ShrinkLoopSpline(const USplineComponent* TargetSpline, float ShrinkValue)
{
	//主要问题在于如何定义收缩
	if (nullptr == TargetSpline)
	{
		return;
	}
	const int32 SplinePointNum = TargetSpline->GetNumberOfSplinePoints();
	if (SplinePointNum < 2)
	{
		return;
	}
	FVector Start = TargetSpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
	FVector End = TargetSpline->GetLocationAtSplinePoint(SplinePointNum - 1, ESplineCoordinateSpace::World);
	FVector Center;
	for (int i = 0; i < SplinePointNum; i++)
	{
		Center += TargetSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World) / (1.0f * SplinePointNum);
	}
	FVector EdgeDir = (Start - End).GetSafeNormal();
	bool bIsClockwise = FVector::DotProduct(FVector::CrossProduct((Start - End), (Center - End)), FVector::UpVector) >
		0.0;
	for (int i = 0; i < SplinePointNum; i++)
	{
		//每个点到Center的连线-ShrinkValue
		FVector ShrinkDir = (Center - TargetSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World)).
			GetSafeNormal() * ShrinkValue;
		const FInterpCurveVector& PositionCurve = TargetSpline->SplineCurves.Position;
		const FInterpCurvePoint<FVector>& PointLoc = PositionCurve.Points[i];
		//PointLoc+=ShrinkDir;
	}
}

bool URoadGeometryUtilities::IsParallel(const FVector& LineAStart, const FVector& LineAEnd, const FVector& LineBStart,
                                        const FVector& LineBEnd, bool bIgnoreZ, const double Tolerance)
{
	//使用DistLine3Line3.h::Line59中的判别方法
	FVector LineADir = (LineAEnd - LineAStart);
	FVector LineBDir = (LineBEnd - LineBStart);
	if (bIgnoreZ)
	{
		LineADir.Z = 0.0;
		LineBDir.Z = 0.0;
	}
	LineADir = LineADir.GetSafeNormal();
	LineBDir = LineBDir.GetSafeNormal();
	const double a01 = LineADir.Dot(LineBDir);
	const double det = FMath::Abs(1 - a01 * a01);
	return det < FMath::Abs(Tolerance);
}

void URoadGeometryUtilities::ResolveTwistySplineSegments(USplineComponent* TargetSpline, bool bTrimAtIntersection)
{
	//能发生扭曲的是k和k+2相对位置颠倒的，对应[k，k+1]和[k+2,K+3]所在分段发生交叉，所以样条至少需要4个顶点
	const int32 SplinePointNum = TargetSpline->GetNumberOfSplinePoints();
	if (4 > SplinePointNum)
	{
		return;
	}
	TArray<int32> TwistyIndexes;
	TArray<FVector> ModifiedLocations;
	//这个基本就是为Spline上有4个点这种特例准备的
	TMap<int32, int32> Visited;
	for (int32 i = 0; i < SplinePointNum - 1; i++)
	{
		//const int32 FirstSegmentStartIndex = i;
		const FVector FirstSegmentStart =
			TargetSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
		const int32 FirstSegmentEndIndex = i + 1;
		const FVector FirstSegmentEnd =
			TargetSpline->GetLocationAtSplinePoint(FirstSegmentEndIndex, ESplineCoordinateSpace::World);
		const int32 SecondSegmentStartIndex = (i + 2) % SplinePointNum;
		const FVector SecondSegmentStart =
			TargetSpline->GetLocationAtSplinePoint(SecondSegmentStartIndex, ESplineCoordinateSpace::World);
		const int32 SecondSegmentEndIndex = (i + 3) % SplinePointNum;
		const FVector SecondSegmentEnd =
			TargetSpline->GetLocationAtSplinePoint(SecondSegmentEndIndex, ESplineCoordinateSpace::World);
		int32 VisitedKey = (i == SplinePointNum - 2 ? i : i < SecondSegmentStartIndex ? i : SecondSegmentStartIndex);
		if (Visited.Contains(VisitedKey))
		{
			continue;
		}
		FVector2D Intersection;
		if (!Get2DIntersection(FVector2D(FirstSegmentStart), FVector2D(FirstSegmentEnd), FVector2D(SecondSegmentStart),
		                       FVector2D(SecondSegmentEnd), Intersection))
		{
			continue;
		}
		Visited.Add(VisitedKey, SecondSegmentStartIndex);
		TwistyIndexes.Add(VisitedKey);
		if (bTrimAtIntersection)
		{
			ModifiedLocations.Emplace(FVector(Intersection, 0.5 * FirstSegmentEnd.Z + 0.5 * SecondSegmentStart.Z));
		}
	}
	if (TwistyIndexes.IsEmpty())
	{
		return;
	}
	if (bTrimAtIntersection)
	{
		ensureMsgf(TwistyIndexes.Num()==ModifiedLocations.Num(), TEXT("InValid Trim Param"));
	}
	//反向遍历否则前边的顺序会被打乱
	for (int32 i = TwistyIndexes.Num() - 1; i >= 0; --i)
	{
		const int32& TargetIndex = TwistyIndexes[i];
		//点数为4的特例，两条边相互相交
		if (SplinePointNum == 4 && TargetIndex == SplinePointNum - 2 && Visited[0] == SplinePointNum - 2)
		{
			if (bTrimAtIntersection)
			{
				TargetSpline->SetLocationAtSplinePoint(1, ModifiedLocations[0], ESplineCoordinateSpace::World, false);
				TargetSpline->RemoveSplinePoint(2);
			}
			else
			{
				FVector Location1 = TargetSpline->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::World);
				TargetSpline->SetLocationAtSplinePoint(
					1, TargetSpline->GetLocationAtSplinePoint(2, ESplineCoordinateSpace::World),
					ESplineCoordinateSpace::World, false);
				TargetSpline->SetLocationAtSplinePoint(
					2, Location1, ESplineCoordinateSpace::World, true);
			}
			return;
		}
		int32 MovementIndex = (TwistyIndexes[i] + 2) % SplinePointNum;
		if (bTrimAtIntersection)
		{
			//把自己后边那个删了，把下一组的挪到交点
			TargetSpline->SetLocationAtSplinePoint(MovementIndex, ModifiedLocations[i], ESplineCoordinateSpace::World,
			                                       false);
			TargetSpline->RemoveSplinePoint(TwistyIndexes[i] + 1, false);
		}
		else
		{
			//交换位置
			FVector Location1 = TargetSpline->GetLocationAtSplinePoint(TwistyIndexes[i] + 1, ESplineCoordinateSpace::World);
			TargetSpline->SetLocationAtSplinePoint(
				TwistyIndexes[i] + 1, TargetSpline->GetLocationAtSplinePoint(MovementIndex, ESplineCoordinateSpace::World),
				ESplineCoordinateSpace::World, false);
			TargetSpline->SetLocationAtSplinePoint(
				MovementIndex, Location1, ESplineCoordinateSpace::World, false);
		}
	}
	TargetSpline->UpdateSpline();
}