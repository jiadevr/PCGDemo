// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EditorComponentUtilities.generated.h"

/**
 * 
 */
UCLASS()
class CITYGENERATOR_API UEditorComponentUtilities : public UObject
{
	GENERATED_BODY()

public:
	/**
 * 辅助函数，生成带有SceneComponent的空Actor
 * @param ActorName ActorLable
 * @param ActorTrans Transform
 * @return 返回生成的对象
 */
	[[nodiscard]] static TObjectPtr<AActor> SpawnEmptyActor(const FString& ActorName, const FTransform& ActorTrans);

	/**
 * 辅助函数，在编辑器中为Actor添加指定类型的Component
 * @param TargetActor 目标Actor
 * @param TargetComponentClass 需要添加的Component类型 
 * @return 返回ActorComponent，根据需要Cast
 */
	static TObjectPtr<UActorComponent> AddComponentInEditor(AActor* TargetActor,
	                                                        TSubclassOf<UActorComponent> TargetComponentClass);

protected:
	static TObjectPtr<UWorld> GetEditorContext();
};
