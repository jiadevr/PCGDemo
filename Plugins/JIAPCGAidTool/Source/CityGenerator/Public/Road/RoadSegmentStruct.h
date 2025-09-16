// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RoadSegmentStruct.generated.h"
class USplineComponent;


/**
 * 记录Spline切分出的多段线信息
 */
USTRUCT(BlueprintType)
struct FSplinePolyLineSegment
{
	GENERATED_BODY()
	FSplinePolyLineSegment()
	{
		//SegmentGlobalIndex++;
	}

public:
	FSplinePolyLineSegment(TWeakObjectPtr<USplineComponent> InSplineRef, int32 InSegmentIndex, int32 InLastSegmentIndex,
	                       const FTransform& InStartTransform,
	                       const FTransform& InEndTransform): OwnerSpline(InSplineRef),
	                                                          SegmentIndex(InSegmentIndex),
	                                                          LastSegmentIndex(InLastSegmentIndex),
	                                                          StartTransform(InStartTransform),
	                                                          EndTransform(InEndTransform)
	{
		GlobalIndex = SegmentGlobalIndex++;
	};

	~FSplinePolyLineSegment()
	{
		OwnerSpline = nullptr;
	}

	/**
	 * 所属的Spline信息
	 */
	UPROPERTY()
	TWeakObjectPtr<USplineComponent> OwnerSpline = nullptr;

	/**
	 * 当前PolyLine的SegmentIndex
	 */
	int32 SegmentIndex = 0;

	/**
	 * 所在Spline被切割出的Segment总数
	 * 这个值是为了排除ClosedLoop最后一点和第一点连接的情况，这边是为了多线程可以不访问Spline对象额外记录的
	 */
	int32 LastSegmentIndex = 0;

	/**
	 * Segment起点，世界空间位置
	 */
	FTransform StartTransform = FTransform::Identity;

	/**
	 * Segment终点，世界空间位置
	 */
	FTransform EndTransform = FTransform::Identity;

	/**
	 * 返回Segment全局ID，用于区分不同Segment
	 * @return 返回ID值
	 */
	uint32 GetGlobalIndex() const { return GlobalIndex; }

protected:
	/**
	 * 全局Segment递增序号
	 */
	static uint32 SegmentGlobalIndex;
	/**
	 * 自身的Segment编号
	 */
	uint32 GlobalIndex = 0;
};

/**
 * 记录Spline的交点信息
 */
USTRUCT(BlueprintType)
struct FSplineIntersection
{
	GENERATED_BODY()

public:
	FSplineIntersection()
	{
	}

	FSplineIntersection(const TArray<TWeakObjectPtr<USplineComponent>>& InIntersectedSplines,
	                    const TArray<int32>& InIntersectedSegmentIndex,
	                    const FVector& InIntersectionPoint): IntersectedSplines(
		                                                         InIntersectedSplines),
	                                                         IntersectedSegmentIndex(InIntersectedSegmentIndex),
	                                                         WorldLocation(InIntersectionPoint)
	{
	}

	~FSplineIntersection()
	{
		IntersectedSplines.Empty();
	}

	
	/**
	 * 相交的样条线引用
	 */
	TArray<TWeakObjectPtr<USplineComponent>> IntersectedSplines;
	/**
	 * 样条线相交处的SegmentIndex
	 */
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly)
	TArray<int32> IntersectedSegmentIndex;
	/**
	 * 交点位置
	 */
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly)
	FVector WorldLocation = FVector::Zero();
};

/**
 * 交点处提取出的Segments信息
 */
USTRUCT(BlueprintType)
struct FIntersectionSegment
{
	GENERATED_BODY()

public:
	FIntersectionSegment(){};

	FIntersectionSegment(TWeakObjectPtr<USplineComponent>& InOwnerSplines, const FVector& InIntersectionEndPointWS,
	                     bool bInIsFlowIn, float InRoadWidth): OwnerSpline(InOwnerSplines),
	                                                           IntersectionEndPointWS(InIntersectionEndPointWS),
	                                                           bIsFlowIn(bInIsFlowIn), RoadWidth(InRoadWidth)
	{
	}
	~FIntersectionSegment()
	{
		OwnerSpline = nullptr;
	};
	
	/**
	 * 所属的SplineComponent
	 */
	UPROPERTY(VisibleInstanceOnly)
	TWeakObjectPtr<USplineComponent> OwnerSpline;
	/**
	 * 沿样条线方向的端点（交点为另一端点）
	 */
	UPROPERTY(BlueprintReadWrite, EditInstanceOnly)
	FVector IntersectionEndPointWS = FVector::Zero();
	/**
	 * 方向（驶入驶出），以Distance判定
	 */
	UPROPERTY(BlueprintReadWrite, EditInstanceOnly)
	bool bIsFlowIn = true;
	/**
	 * 道路宽度
	 */
	UPROPERTY(BlueprintReadWrite, EditInstanceOnly)
	float RoadWidth = 0;
};
