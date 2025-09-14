// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RoadSegmentStruct.generated.h"
class USplineComponent;


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

	UPROPERTY()
	TWeakObjectPtr<USplineComponent> OwnerSpline = nullptr;

	int32 SegmentIndex = 0;
	//这个值是为了排除ClosedLoop最后一点和第一点连接的情况，这边是为了多线程可以不访问Spline对象额外记录的
	int32 LastSegmentIndex = 0;

	FTransform StartTransform = FTransform::Identity;

	FTransform EndTransform = FTransform::Identity;

	uint32 GetGlobalIndex() const { return GlobalIndex; }

protected:
	static uint32 SegmentGlobalIndex;
	uint32 GlobalIndex = 0;
};

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

	//UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly)
	TArray<TWeakObjectPtr<USplineComponent>> IntersectedSplines;
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly)
	TArray<int32> IntersectedSegmentIndex;
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly)
	FVector WorldLocation = FVector::Zero();
};

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
	UPROPERTY(VisibleInstanceOnly)
	TWeakObjectPtr<USplineComponent> OwnerSpline;
	UPROPERTY(BlueprintReadWrite, EditInstanceOnly)
	FVector IntersectionEndPointWS = FVector::Zero();
	UPROPERTY(BlueprintReadWrite, EditInstanceOnly)
	bool bIsFlowIn = true;
	UPROPERTY(BlueprintReadWrite, EditInstanceOnly)
	float RoadWidth = 0;
};
