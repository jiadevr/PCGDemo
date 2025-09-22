// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "CityGeneratorSubSystem.generated.h"

class URoadGeneratorSubsystem;
class USplineComponent;
/**
 * 
 */
UCLASS()
class CITYGENERATOR_API UCityGeneratorSubSystem : public UEditorSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	FDelegateHandle ActorAddedHandle;
	FDelegateHandle ActorRemovedHandle;
	virtual void Deinitialize() override;
protected:
	/**
	 * 绑定GEditor的Actor增删多播委托，设置bNeedRefreshSplineData=true,用于在必要时更新Spline列表
	 * @param LevelActor 传入的Actor对象，调用该委托在实际删除对象前可以获得有效的Actor指针
	 */
	void OnSplineActorChanged(AActor* LevelActor);
	bool bNeedRefreshSplineData=true;
	
public:
#pragma region  UserSplineManage

	/**
	 * 获取场景中的SplineComponents，可选传入ActorTag和ComponentTag作为前置筛选，结果覆盖CityGeneratorSplineArray;
	 * @param OptionalActorTag 可选项，在获取Actor时进行前置过滤
	 * @param OptionalCompTag 可选项，在从Actor中获取SplineComponent时进行过滤
	 */
	UFUNCTION(BlueprintCallable)
	void CollectAllSplines(const FName OptionalActorTag = "", const FName OptionalCompTag = "");

	/**
	 * 序列化Spline信息，用于备份
	 * @param FileName 保存文件名称，不需要后缀，以.json格式保存；有重名文件时有对话框提示
	 * @param FilePath 保存文件路径，为空时保存在项目Saved下
	 * @param bSaveActorTag 是否保存ActorTag
	 * @param bSaveCompTag 是否保存SplineCompTag
	 */
	UFUNCTION(BlueprintCallable)
	void SerializeSplines(const FString& FileName, const FString& FilePath = "", bool bSaveActorTag = false,
	                      bool bSaveCompTag = false);

	/**
	 * 反序列化SPline信息，用于还原。还原结果为Actor带有SceneComponent（Root）和SplineComponent
	 * @param FileFullPath 读取文件名称，需要为序列化产生的.json文件；
	 * @param bTryParseActorTag 是否尝试解析序列化数据中的ActorTag并添加到新生成的Actor
	 * @param bTryParseCompTag 是否尝试解析序列化数据中的ComponentTag并添加到新生成的Actor
	 * @param bAutoCollectAfterSpawn 是否在全部生成完毕时刷新当前Spline信息，即最后运行UCityGeneratorSubSystem::CollectAllSplines
	 */
	UFUNCTION(BlueprintCallable)
	void DeserializeSplines(const FString& FileFullPath, bool bTryParseActorTag = false, bool bTryParseCompTag = false,
	                        bool bAutoCollectAfterSpawn = false);

	/**
	 * 向已经存在的Actor添加SplineComponent并设置其参数
	 * @param TargetActor 目标Actor
	 * @param PointsType 点类型，Curve、Linear
	 * @param PointsLoc 点位置数组
	 * @param PointTangent 点切线数组
	 * @param PointRotator 点旋转数组
	 * @param bIsCloseLoop 是否闭合样条
	 */
	TObjectPtr<USplineComponent> AddSplineCompToExistActor(TObjectPtr<AActor> TargetActor,
	                                                       const TArray<int32>& PointsType,
	                                                       const TArray<FVector>& PointsLoc,
	                                                       const TArray<FVector>& PointTangent,
	                                                       const TArray<FRotator>& PointRotator,
	                                                       const bool bIsCloseLoop);


	/**
	 * 接口函数，用于获取场景中的样条对象
	 * @return 场景样条对象弱指针，返回前已经及逆行有效性检验
	 */
	TSet<TWeakObjectPtr<USplineComponent>> GetSplines();

protected:
	TObjectPtr<UWorld> GetEditorContext() const;


#pragma endregion UserSplineManage
	
#pragma region GenerateRoad

public:
	UFUNCTION(BlueprintCallable)
	void GenerateRoads();

protected:
	UPROPERTY()
	TWeakObjectPtr<URoadGeneratorSubsystem> RoadSubsystem;

	TWeakObjectPtr<URoadGeneratorSubsystem> GetRoadGeneratorSystem();
#pragma endregion GenerateRoad

protected:
	UPROPERTY()
	TSet<TWeakObjectPtr<USplineComponent>> CityGeneratorSplineSet;
};
