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

	/**
	 * 在给定样条上连续放置建筑
	 * @param InAllEdges 所有样条线分段，用于计算死区
	 * @param InTargetEdgeIndex 需要放置建筑的分段索引
	 * @param InAllBuildingExtent 全部建筑尺寸盒子
	 * @param PlacedBuildings 已放置的建筑，原位增加
	 */
	void PlaceBuildingsAtEdge(const TArray<FPlaceableBlockEdge>& InAllEdges, int32 InTargetEdgeIndex,
	                          const TArray<FVector>& InAllBuildingExtent,
	                          TArray<FPlacedBuilding>& PlacedBuildings);

protected:
	UPROPERTY()
	TObjectPtr<UBuildingDimensionsConfig> BuildingConfig;

	FString BuildingConfigPath = "/JIAPCGAidTool/CityGeneratorContent/BuildingConfigs/PD_SuburbConfig";
	/*TObjectPtr<UBuildingDimensionsConfig> CBDConfig; //HeroBuilding
	TObjectPtr<UBuildingDimensionsConfig> DowntownConfig; //市中心
	TObjectPtr<UBuildingDimensionsConfig> UptownConfig; //住宅区
	TObjectPtr<UBuildingDimensionsConfig> SuburbConfig; //郊区*/

	void InitialConfigDataAsset();

	void MarkLocationOnEdge(const FPlaceableBlockEdge& TargetEdge, float Distance, const FColor& DebugColor,
	                        bool bFromStart = true, float InArrowSize = 200.0f);

	//计算相邻段（一般是前一段）到当前端InCurrentEdgeIndex的死区，即相邻边上建筑边投影到当前边的距离
	//当InCurrentEdgeIndex==InAllPlaceableEdges.Num()返回第0段在最后一段的死区
	float GetDeadLength(const TArray<FPlaceableBlockEdge>& InAllPlaceableEdges, int32 InCurrentEdgeIndex,
	                    const TArray<FPlacedBuilding>& InPlacedBuildings) const;

	//测试内容
	TArray<FPlaceableBlockEdge> PlaceableEdges_Test;

	int32 CurrentEdgeIndex = 0;
	//FColor DebugColorOfSingleLine = FColor::Red;
	TArray<FVector> BuildingsExtentArray;
	//float DistanceUsedInSingleLine = 0.0f;
	//TSet<int32> UsedIDInSingleLine;
	TArray<FPlacedBuilding> PlacedBuildingsGlobal;

	UFUNCTION(BlueprintCallable)
	void ClearPlacedBuildings();
};
