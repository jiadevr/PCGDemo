// Fill out your copyright notice in the Description page of Project Settings.


#include "SplineUtilities.h"
#include "Components/SplineComponent.h"

float USplineUtilities::GetSplineSegmentLength(const USplineComponent* TargetSpline, int32 SegmentIndex)
{
	if (SegmentIndex < 0 || SegmentIndex >= TargetSpline->GetNumberOfSplineSegments())
	{
		return 0;
	}
	if (SegmentIndex == TargetSpline->GetNumberOfSplinePoints() - 1)
	{
		if (TargetSpline->IsClosedLoop())
		{
			return TargetSpline->GetSplineLength() - TargetSpline->
				GetDistanceAlongSplineAtSplinePoint(SegmentIndex);
		}
		else
		{
			return 0;
		}
	}
	return TargetSpline->GetDistanceAlongSplineAtSplinePoint(SegmentIndex + 1) - TargetSpline->
		GetDistanceAlongSplineAtSplinePoint(SegmentIndex);
}
