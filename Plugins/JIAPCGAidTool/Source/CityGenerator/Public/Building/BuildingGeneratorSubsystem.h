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
	/**
	 * 主函数，对外接口
	 * 使用随机生成的建筑信息（BuildingConfig）填充指定样条线的边
	 * @param TargetSpline 目标样条线，建议使用Linear
	 */
	UFUNCTION(BlueprintCallable)
	void PlaceBuildingAlongSpline(USplineComponent* TargetSpline);

	/**
	 * 配置随机生成建筑配置
	 * @param InTargetConfig 
	 */
	UFUNCTION(BlueprintCallable)
	void SetBuildingConfig(UBuildingDimensionsConfig* InTargetConfig) { BuildingConfig = InTargetConfig; };

	/**
	 * 根据配置的建筑信息（BuildingConfig）生成随机建筑信息
	 * @param Count 生成的随机建筑信息数量
	 * @return 随机建筑数组（未排序）
	 */
	UFUNCTION(BlueprintCallable)
	TArray<FVector> GetRandomBuildingConfig(int32 Count = 10);

	/**
	 * 测试函数
	 * 每次点击在已经初始化（PlaceBuildingAlongSpline）的样条上放置建筑，用于检测碰撞、死区
	 * 受到CityGenerator.Building.FillAllEdge=false控制，对应函数为PlaceBuildingsAtEdge
	 */
	UFUNCTION(BlueprintCallable)
	void Test_PlaceBuildingAtEdgeManually();

	//测试函数
	//输入两个静态网格体调用圆距离、AABB、OBB碰撞检测
	UFUNCTION(BlueprintCallable)
	void Test_OverlappingUsingSelectedMeshBox(const AStaticMeshActor* InA, const AStaticMeshActor* InB);

protected:
	UPROPERTY()
	TObjectPtr<UBuildingDimensionsConfig> BuildingConfig;

	FString BuildingConfigPath = "/JIAPCGAidTool/CityGeneratorContent/BuildingConfigs/PD_SuburbConfig";
	/*TObjectPtr<UBuildingDimensionsConfig> CBDConfig; //HeroBuilding
	TObjectPtr<UBuildingDimensionsConfig> DowntownConfig; //市中心
	TObjectPtr<UBuildingDimensionsConfig> UptownConfig; //住宅区
	TObjectPtr<UBuildingDimensionsConfig> SuburbConfig; //郊区*/

	void InitialConfigDataAsset();

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

	/**
	 * 辅助Debug函数，在给定Edge上标记长度
	 * @param TargetEdge 目标Spline离散边
	 * @param Distance 从起点/终点起的距离
	 * @param DebugColor 绘制线条颜色
	 * @param bFromStart 从起点/终点起计算距离
	 * @param InArrowSize 绘制的ArrowLine中Arrow的大小
	 */
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
