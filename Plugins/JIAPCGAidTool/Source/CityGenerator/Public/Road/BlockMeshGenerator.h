// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MeshGeneratorInterface.h"
#include "Components/ActorComponent.h"
#include "BlockMeshGenerator.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CITYGENERATOR_API UBlockMeshGenerator : public UActorComponent, public IMeshGeneratorInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UBlockMeshGenerator();

public:
	/**
	 * 对外接口，接受外部传入的轮廓点
	 * @param InSweepPath 轮廓点数组
	 */
	void SetSweepPath(const TArray<FVector>& InSweepPath);
	
	virtual void SetMeshComponent(class UDynamicMeshComponent* InMeshComponent) override;
	
	virtual bool GenerateMesh() override;

protected:
	TArray<FVector2D> SweepPath;
	
	static int32 BlockGlobalIndex;
};
