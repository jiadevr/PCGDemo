// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "RoadGeneratorSubsystem.generated.h"

class USplineComponent;
/**
 * 
 */
UENUM()
enum class ELaneType:uint8
{
	SingleWay,
	TwoLaneTwoWay,
	MAX
};

USTRUCT()
struct FLaneMeshInfo
{
	GENERATED_BODY()

public:
	FLaneMeshInfo()
	{
		CrossSectionCoord = FLaneMeshInfo::GetRectangle2DCoords(400.0, 20.0);
	}

	FLaneMeshInfo(const float CrossSectionWidth, const float CrossSectionHeight, const float Length = 500.0f)
	{
		CrossSectionCoord = FLaneMeshInfo::GetRectangle2DCoords(CrossSectionWidth, CrossSectionHeight);
		SampleLength = Length;
	}

	TArray<FVector2D> CrossSectionCoord;
	float SampleLength = 500.0;

protected:
	static TArray<FVector2D> GetRectangle2DCoords(float Width, float Height, bool Clockwise = true)
	{
		TArray<FVector2D> Rectangle2DCoords;
		Rectangle2DCoords.SetNum(4);
		TArray<FVector2D> UnitShape{{0.5, 0.5}, {0.5, -0.5}, {-0.5, -0.5}, {-0.5, 0.5}};
		for (int i = 0; i < UnitShape.Num(); ++i)
		{
			Rectangle2DCoords[i] = FVector2D(Width, Height) * UnitShape[i];
		}
		return MoveTemp(Rectangle2DCoords);
	}
};

UCLASS()
class CITYGENERATOR_API URoadGeneratorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
#pragma region GenerateRoad

public:
	UFUNCTION(BlueprintCallable)
	void GenerateSingleRoadBySweep(const USplineComponent* TargetSpline,
	                               const ELaneType LaneTypeEnum = ELaneType::SingleWay);

	UPROPERTY(BlueprintReadWrite)
	float CurveResampleLengthInCM = 500.0f;

	float GetSplineSegmentLength(const USplineComponent* TargetSpline, int32 StartPointIndex);

	bool ResampleSamplePoint(const USplineComponent* TargetSpline, TArray<FTransform>& OutResampledTransform,
	                         const float MaxResampleDistance, double StartShrink = 0.0,
	                         double EndShrink = 0.0);

	UFUNCTION(BlueprintCallable)
	TArray<FVector> TestNativeSubdivisionFunction(const USplineComponent* TargetSpline, const int32 Index);

	TArray<FVector> IntersectionLocation;

protected:
	UPROPERTY()
	TMap<ELaneType, FLaneMeshInfo> RoadPresetMap;


#pragma endregion GenerateRoad
};
