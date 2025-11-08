// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "BuildingGeneratorSubsystem.generated.h"


class UBuildingDimensionsConfig;
class USplineComponent;

UCLASS()
class CITYGENERATOR_API UBuildingGeneratorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; };
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	UFUNCTION(BlueprintCallable)
	void PlaceBuildingAlongSpline(const USplineComponent* TargetSpline);

	UFUNCTION(BlueprintCallable)
	void SetBuildingConfig(UBuildingDimensionsConfig* InTargetConfig) { BuildingConfig = InTargetConfig; };

	UFUNCTION(BlueprintCallable)
	TArray<FVector> GetRandomBuildingConfig(int32 Count = 10);

protected:
	UPROPERTY()
	TObjectPtr<UBuildingDimensionsConfig> BuildingConfig;

	FString BuildingConfigPath = "/JIAPCGAidTool/CityGeneratorContent/BuildingConfigs/PD_SuburbConfig";
	/*TObjectPtr<UBuildingDimensionsConfig> CBDConfig; //HeroBuilding
	TObjectPtr<UBuildingDimensionsConfig> DowntownConfig; //市中心
	TObjectPtr<UBuildingDimensionsConfig> UptownConfig; //住宅区
	TObjectPtr<UBuildingDimensionsConfig> SuburbConfig; //郊区*/

	void InitialConfigDataAsset();
};
