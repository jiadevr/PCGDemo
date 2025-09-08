// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoadDataComp.generated.h"


class USplineComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CITYGENERATOR_API URoadDataComp : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URoadDataComp();

	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	int32 DebugIndex;
	UFUNCTION(CallInEditor,BlueprintCallable)
	void DrawDebugElemOnSweepPoint();

	UPROPERTY(BlueprintReadOnly,VisibleInstanceOnly)
	TArray<FTransform> SweepPointsTrans;

	UPROPERTY(BlueprintReadOnly,VisibleInstanceOnly)
	TWeakObjectPtr<USplineComponent> ReferenceSpline;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
};
