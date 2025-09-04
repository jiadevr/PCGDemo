// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "CityGeneratorSubSystem.generated.h"

class USplineComponent;
/**
 * 
 */
UCLASS()
class CITYGENERATOR_API UCityGeneratorSubSystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
#pragma region  Base
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
	 * @param bForceRecollect 保存前是否刷新当前Spline信息，即首先运行UCityGeneratorSubSystem::CollectAllSplines
	 */
	UFUNCTION(BlueprintCallable)
	void SerializeSplines(const FString& FileName, const FString& FilePath = "", bool bSaveActorTag = false,
	                      bool bSaveCompTag = false,
	                      bool bForceRecollect = false);

	UFUNCTION(BlueprintCallable)
	void DeserializeSplines(const FString& FileFullPath, bool bTryParseActorTag = false, bool bTryParseCompTag = false,
	                        bool bAutoCollectAfterSpawn = false);


	TObjectPtr<AActor> SpawnEmptyActor(const FString& ActorName, const FTransform& ActorTrans);

	void AddSplineCompToExistActor(TObjectPtr<AActor> TargetActor, const TArray<int32>& PointsType,
	                               const TArray<FVector>& PointsLoc,
	                               const TArray<FVector>& PointTangent, const TArray<FRotator>& PointRotator,const bool bIsCloseLoop);

	TObjectPtr<UActorComponent> AddComponentInEditor(AActor*TargetActor,TSubclassOf<UActorComponent> TargetComponentClass);
#pragma endregion Base

	UFUNCTION(BlueprintCallable)
	void GenerateSingleRoadBySweep(const USplineComponent* TargetSpline, const TArray<FVector2D>& SweepShape);

protected:
	UPROPERTY()
	TArray<TWeakObjectPtr<USplineComponent>> CityGeneratorSplineArray;

	TObjectPtr<UWorld> GetEditorContext() const;
};
