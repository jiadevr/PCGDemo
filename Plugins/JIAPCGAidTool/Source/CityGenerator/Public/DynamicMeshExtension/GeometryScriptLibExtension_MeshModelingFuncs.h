// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScriptLibExtension_MeshModelingFuncs.generated.h"

class UDynamicMesh;

USTRUCT(BlueprintType)
struct FGeometryScriptMeshShrinkVerticesOptions
{
	GENERATED_BODY()

	/** 内缩距离（正值表示向模型内部移动） */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Options", meta = (ClampMin = 0))
	float Distance = 5.f;

	/** 若为空则处理整个网格 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Options")
	FGeometryScriptMeshSelection Selection;
};

UCLASS()
class CITYGENERATOR_API UGeometryScriptLibExtension_MeshModelingFuncs : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshModeling",
		meta = (DisplayName = "Apply Mesh Shrink Vertices"))
	static UDynamicMesh* ApplyMeshShrinkVertices(
		UDynamicMesh* TargetMesh,
		const FGeometryScriptMeshShrinkVerticesOptions& Options,
		UGeometryScriptDebug* Debug);
};
