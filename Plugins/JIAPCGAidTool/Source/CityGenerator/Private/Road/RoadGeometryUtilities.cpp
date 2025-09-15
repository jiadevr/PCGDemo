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

void URoadGeometryUtilities::SortPointCounterClockwise(const FVector2D& Center, TArray<FVector2D>& ArrayToSort)
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
