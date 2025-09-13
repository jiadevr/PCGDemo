// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MeshGeneratorInterface.h"
#include "Components/ActorComponent.h"
#include "RoadMeshGenerator.generated.h"


class USplineComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CITYGENERATOR_API URoadMeshGenerator : public UActorComponent,public IMeshGeneratorInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URoadMeshGenerator();

	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	int32 DebugIndex;
	UFUNCTION(CallInEditor,BlueprintCallable)
	void DrawDebugElemOnSweepPoint();

	UPROPERTY(BlueprintReadOnly,VisibleInstanceOnly)
	TArray<FTransform> SweepPointsTrans;

	UPROPERTY(BlueprintReadOnly,VisibleInstanceOnly)
	TWeakObjectPtr<USplineComponent> ReferenceSpline;

	virtual bool GenerateMesh() override;
	virtual void SetMeshComponent(class UDynamicMeshComponent* InMeshComponent) override;
};
