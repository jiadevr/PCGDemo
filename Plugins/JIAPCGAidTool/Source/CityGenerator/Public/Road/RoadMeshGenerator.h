// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MeshGeneratorInterface.h"
#include "RoadSegmentStruct.h"
#include "Components/ActorComponent.h"
#include "RoadMeshGenerator.generated.h"


class USplineComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CITYGENERATOR_API URoadMeshGenerator : public UActorComponent, public IMeshGeneratorInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URoadMeshGenerator();


	UFUNCTION(CallInEditor, BlueprintCallable)
	void DrawDebugElemOnSweepPoint();

	/**
	 * 设置参考/归属样条线,持有弱引用
	 * @param InReferenceSpline 所属Spline
	 */
	void SetReferenceSpline(TWeakObjectPtr<USplineComponent> InReferenceSpline);

	/**
	 * 设置道路类型
	 * @param InRoadType 道路枚举值，COLLECTORROADS=5m，ARTERIALROADS=10m,EXPRESSWAYS=20m
	 */
	void SetRoadType(ELaneType InRoadType);

	/**
	 * 设置道路Segment和连接点信息，分别传入，不要该函数前合并连接点和原本连续的Segments；在该函数中会进行合并
	 * @param InRoadWithConnect 
	 */
	void SetRoadInfo(const FRoadSegmentsGroup& InRoadWithConnect);

	virtual bool GenerateMesh() override;
	virtual void SetMeshComponent(class UDynamicMeshComponent* InMeshComponent) override;


	[[nodiscard]] TArray<FVector> GetRoadEdgePoints(bool bForwardOrderDir = true);

	[[nodiscard]] TArray<FVector> GetSplineControlPointsInRoadRange(bool bForwardOrderDir = true);

	void GetConnectionOrderOfIntersection(int32& OutLocFromIndex, int32& OutLocEndIndex) const;

protected:
	bool bIsLocalSpace = false;
	/**
	 * 世界空间转换局部空间，原位转换，配合上面的bIsLocalSpace进行标记，仅在生成时进行一次性转换（便于分帧操作）
	 * @param InActorTransform 
	 */
	void ConvertPointToLocalSpace(const FTransform& InActorTransform);
	/**
	 * 记录道路分段的坐标，默认传入世界空间坐标在生成前会原位转换为局部坐标
	 */
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly)
	TArray<FTransform> SweepPointsTrans;

	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly)
	TWeakObjectPtr<USplineComponent> ReferenceSpline;

	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly)
	FLaneMeshInfo RoadInfo{500.0f};

	static int32 RoadGlobalIndex;

	int32 StartToIntersectionIndex = INT32_ERROR;
	int32 EndToIntersectionIndex = INT32_ERROR;
};
