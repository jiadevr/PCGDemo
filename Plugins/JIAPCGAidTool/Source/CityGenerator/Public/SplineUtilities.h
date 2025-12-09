// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SplineUtilities.generated.h"

class USplineComponent;
/**
 * 
 */
UCLASS()
class CITYGENERATOR_API USplineUtilities : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static float GetSplineSegmentLength(const USplineComponent* TargetSpline, int32 SegmentIndex);
};
