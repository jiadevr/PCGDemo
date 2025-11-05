// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
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
	static bool Get2DIntersection(const FVector2D& InSegmentAStart, const FVector2D& InSegmentAEnd,
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

	/**
	 * 以给定中心为原点**顺时针**排序给定点数组
	 * 使用Atan2实现，修改值域范围为[0,2pi]，世界X轴正方向为0(2pi)，Y轴正方向为正旋转方向(pi/2)
	 * @param Center 
	 * @param ArrayToSort 
	 */
	static void SortPointClockwise(const FVector2D& Center, TArray<FVector2D>& ArrayToSort);

	/**
	 * 使用Shoelace法计算有序（顺时针、逆时针）顶点围成的多边形面积
	 * @param SortedVertex 有序顶点数组
	 * @return 面积绝对值
	 */
	static double GetAreaOfSortedPoints(const TArray<FVector2D>& SortedVertex);

	/**
	 * 原位简化传入的点集以生成样条控制点，采用长度和角度双控制
	 * 使用长度简化应对拐角处细分值
	 * 使用角度控制应对长直线
	 * @param SplinePoints 需要进行简化的点
	 * @param bIgnoreZ 忽略Z轴数值
	 * @param DisThreshold 长度简化阈值cm
	 * @param AngleThreshold 角度简化阈值°
	 */
	static void SimplifySplinePointsInline(TArray<FVector>& SplinePoints, bool bIgnoreZ = true,
	                                       const float DisThreshold = 200.0f, const float AngleThreshold = 2.5f);

	static void ShrinkLoopSpline(const USplineComponent* TargetSpline, float ShrinkValue);
	
	/**
	 * 判断两条线段是否平行
	 * @param LineAStart 线段A起点
	 * @param LineAEnd 线段A终点
	 * @param LineBStart 线段B起点
	 * @param LineBEnd 线段B终点
	 * @param bIgnoreZ 是否忽略Z数值
	 * @param Tolerance 平行判断阈值，对应两个矢量夹角Sin计算结果
	 * @return 给定两个线段是否平行
	 */
	static bool IsParallel(const FVector& LineAStart, const FVector& LineAEnd, const FVector& LineBStart,
	                       const FVector& LineBEnd, bool bIgnoreZ = true, const double Tolerance = 1e-08);

	/**
	 * 整理因为Shrink扭曲的样条控制点，表现k和k+2相对位置颠倒，[k，k+1]和[k+2,K+3]所在分段发生交叉
	 * @param TargetSpline 目标样条线
	 * @param bTrimAtIntersection 是否在交点处焊接样条；true 移动其中一点并删除另一点，false交换两者位置
	 */
	static void ResolveTwistySplineSegments(USplineComponent* TargetSpline,bool bTrimAtIntersection=true);
};
