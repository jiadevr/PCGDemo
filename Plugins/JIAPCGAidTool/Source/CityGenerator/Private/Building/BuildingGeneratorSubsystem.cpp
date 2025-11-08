// Fill out your copyright notice in the Description page of Project Settings.


#include "Building/BuildingGeneratorSubsystem.h"

#include "NotifyUtilities.h"
#include "Building/BuildingDimensionsConfig.h"
#include "Subsystems/EditorAssetSubsystem.h"

void UBuildingGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UBuildingGeneratorSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UBuildingGeneratorSubsystem::PlaceBuildingAlongSpline(const USplineComponent* TargetSpline)
{
	//0.基础检查
	if (nullptr == TargetSpline)
	{
		return;
	}
	//1.构造离散PlaceableEdge

	//2.随机生成建筑三维，对建筑维度进行排序，建立大顶堆
	TArray<FVector> BuildingsDimensions = GetRandomBuildingConfig();
	//长度优先，深度第二，高度第三
	UE_LOG(LogTemp,Display,TEXT("Begin Sort BuildingDimensions"))
	BuildingsDimensions.Sort([](const FVector& A, const FVector& B)
	{
		if (A.X != B.X)
		{
			return A.X > B.X;
		}
		else
		{
			if (A.Y != B.Y)
			{
				return A.Z > B.Z;
			}
			return A.Y > B.Y;
		}
	});
	UE_LOG(LogTemp,Display,TEXT("End Sort BuildingDimensions"))
	//3.建立局部网格处理碰撞
	//Sphere空间距离检测、AABB检测、OBB检测
	//4.处理每条边放置
	//贪心，01背包问题
	
	return;
}

TArray<FVector> UBuildingGeneratorSubsystem::GetRandomBuildingConfig(int32 Count)
{
	TArray<FVector> BuildingBoxes;
	if (nullptr == BuildingConfig)
	{
		InitialConfigDataAsset();
		if (nullptr == BuildingConfig)
		{
			return BuildingBoxes;
		}
	}
	for (int32 i = 0; i < Count; i++)
	{
		BuildingBoxes.Emplace(BuildingConfig.Get()->GetRandomDimension());
	}
	return BuildingBoxes;
}

void UBuildingGeneratorSubsystem::InitialConfigDataAsset()
{
	UEditorAssetSubsystem* AssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	if (AssetSubsystem && AssetSubsystem->DoesAssetExist(BuildingConfigPath))
	{
		UObject* BuildingConfigDA = AssetSubsystem->LoadAsset(BuildingConfigPath);
		BuildingConfig = Cast<UBuildingDimensionsConfig>(BuildingConfigDA);
	}
	ensureMsgf(nullptr!=BuildingConfig, TEXT("Load Building Config Failed"));
}
