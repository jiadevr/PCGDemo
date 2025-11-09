// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Kismet/KismetMathLibrary.h"
#include "BuildingDimensionsConfig.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class CITYGENERATOR_API UBuildingDimensionsConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ConfigName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=10, ClampMax=200))
	int32 MinimalLengthInM;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=10, ClampMax=200))
	int32 MaximalLengthInM;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=10, ClampMax=200))
	int32 MinimalDepthInM;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=10, ClampMax=200))
	int32 MaximalDepthInM;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=3, ClampMax=200))
	int32 MinimalHeightInM;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=3, ClampMax=200))
	int32 MaximalHeightInM;

	FRandomStream RandomStream;

	static void NormalizeValue(int32& InValue, const int32 SnapValue)
	{
		InValue /= SnapValue;
		InValue *= SnapValue;
	}

public:
	void SetRandomSeed(int32 InSeed)
	{
		RandomStream = FRandomStream(InSeed);
	}

	UFUNCTION(BlueprintCallable, Category="BuildingDimensions")
	FVector GetRandomHalfDimension() const
	{
		FVector Dimension;
		int32 RandomLengthInM = UKismetMathLibrary::RandomIntegerInRangeFromStream(
			RandomStream, MinimalLengthInM, MaximalLengthInM);
		NormalizeValue(RandomLengthInM, 5);
		Dimension.X = RandomLengthInM;
		int32 RandomDepthInM = UKismetMathLibrary::RandomIntegerInRangeFromStream(
			RandomStream, MinimalDepthInM, MaximalDepthInM);
		NormalizeValue(RandomDepthInM, 5);
		Dimension.Y = RandomDepthInM;
		int32 RandomHeightInM = UKismetMathLibrary::RandomIntegerInRangeFromStream(
			RandomStream, MinimalHeightInM, MaximalHeightInM);
		NormalizeValue(RandomHeightInM, 3);
		Dimension.Z = RandomHeightInM;
		Dimension *= 50.0;
		return Dimension;
	}
	UFUNCTION(BlueprintCallable,Category="BuildingDimensions")
	FName GetConfigName() const{return ConfigName;}
};
