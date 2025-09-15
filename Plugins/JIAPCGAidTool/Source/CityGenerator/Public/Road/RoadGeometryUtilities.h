// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RoadGeometryUtilities.generated.h"

class USplineComponent;
/**
 * 
 */
UCLASS()
class CITYGENERATOR_API URoadGeometryUtilities : public UObject
{
	GENERATED_BODY()
public:
	/**
	 * 使用参数法计算传入的直线段SegmentA和SegmentB在Start-End范围内的交点，使用快速排斥算法剪枝
	 * @param InSegmentAStart 待测试直线段A起点
	 * @param InSegmentAEnd 待测试直线段A终点
	 * @param InSegmentBStart 待测试直线段B起点
	 * @param InSegmentBEnd 待测试直线段B终点
	 * @param OutIntersection 交点（如存在）
	 * @return SegmentA和SegmentB在Start-End范围内是否存在交点
	 */
	static 	bool Get2DIntersection(const FVector2D& InSegmentAStart, const FVector2D& InSegmentAEnd,
						   const FVector2D& InSegmentBStart, const FVector2D& InSegmentBEnd,
						   FVector2D& OutIntersection);


	/**
	 * 使用贝塞尔样条拟合、使用Newton-Raphson法计算两条样条线的交点
	 * @param TargetSplineA 待测试样条A
	 * @param TargetSplineB 待测试样条B
	 * @param IntersectionsIn2DSpace 交点数组
	 * @return 待测试样条A、B在样条全长内是否存在交点
	 */
	static bool Get2DIntersection(USplineComponent* TargetSplineA, USplineComponent* TargetSplineB,
						   TArray<FVector2D>& IntersectionsIn2DSpace);

	static void SortPointCounterClockwise(const FVector2D& Center,TArray<FVector2D>& ArrayToSort);
};
