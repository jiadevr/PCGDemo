// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BuildingPlacementStruct.h"
#include "EditorSubsystem.h"
#include "Logging/LogMacros.h"
#include "BuildingGeneratorSubsystem.generated.h"


class UBuildingDimensionsConfig;
class USplineComponent;
DECLARE_LOG_CATEGORY_EXTERN(LogCityGenerator, Log, All);

UCLASS()
class CITYGENERATOR_API UBuildingGeneratorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; };
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	UFUNCTION(BlueprintCallable)
	void PlaceBuildingAlongSpline(USplineComponent* TargetSpline);

	UFUNCTION(BlueprintCallable)
	void SetBuildingConfig(UBuildingDimensionsConfig* InTargetConfig) { BuildingConfig = InTargetConfig; };

	UFUNCTION(BlueprintCallable)
	TArray<FVector> GetRandomBuildingConfig(int32 Count = 10);

	//测试函数输入两个静态网格体判断
	UFUNCTION(BlueprintCallable)
	void TestOverlappingUsingSelectedMeshBox(const AStaticMeshActor* InA, const AStaticMeshActor* InB);

	UFUNCTION(BlueprintCallable)
	void TestAddABuilding();

protected:
	UPROPERTY()
	TObjectPtr<UBuildingDimensionsConfig> BuildingConfig;

	FString BuildingConfigPath = "/JIAPCGAidTool/CityGeneratorContent/BuildingConfigs/PD_SuburbConfig";
	/*TObjectPtr<UBuildingDimensionsConfig> CBDConfig; //HeroBuilding
	TObjectPtr<UBuildingDimensionsConfig> DowntownConfig; //市中心
	TObjectPtr<UBuildingDimensionsConfig> UptownConfig; //住宅区
	TObjectPtr<UBuildingDimensionsConfig> SuburbConfig; //郊区*/

	void InitialConfigDataAsset();

	bool CanPlaceNewBuilding(const FPlacedBuilding& NewBuilding, const TArray<FPlacedBuilding>& Placed);

	//测试内容
	TWeakObjectPtr<USplineComponent> LastOperatorSpline;
	TArray<FPlaceableBlockEdge> PlaceableEdges_Test;

	int32 CurrentEdgeIndex = -1;
	FColor DebugColorOfSingleLine = FColor::Red;
	TArray<FVector> BuildingsExtentArray;
	float DistanceUsedInSingleLine = 0.0f;
	TSet<int32> UsedIDInSingleLine;
	TArray<FPlacedBuilding> PlacedBuildingsGlobal;

	void MarkLocationOnEdge(const FPlaceableBlockEdge& TargetEdge, float Distance, const FColor& DebugColor);

	UFUNCTION(BlueprintCallable)
	void ClearPlacedBuildings();

	//计算相邻段（一般是前一段）到当前端InCurrentEdgeIndex的死区
	//当InCurrentEdgeIndex==InAllPlaceableEdges.Num()返回第0段在最后一段的死区
	float GetDeadLength(const TArray<FPlaceableBlockEdge>& InAllPlaceableEdges, int32 InCurrentEdgeIndex,
	                    const TArray<FPlacedBuilding>& InPlacedBuildings) const;
};
