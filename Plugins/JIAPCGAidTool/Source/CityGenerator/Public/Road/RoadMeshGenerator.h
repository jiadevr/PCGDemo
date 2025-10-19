// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MeshGeneratorInterface.h"
#include "RoadSegmentStruct.h"
#include "Components/ActorComponent.h"
#include "RoadMeshGenerator.generated.h"


class USplineComponent;

UENUM(BlueprintType)
enum class ECoordOffsetType:uint8
{
	ROADCENTER,
	LEFTEDGE,
	CUSTOM
};

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


	/**
	 * 获取道路给定朝向左侧边界的点
	 * @param bForwardOrderDir 朝向是否和道路构建方向一致，配合GetConnectionOrderOfIntersection()使用
	 * @return 道路左边界点数组
	 */
	[[nodiscard]] TArray<FVector> GetRoadEdgePoints(bool bForwardOrderDir = true);

	//UFUNCTION(BlueprintCallable)
	[[nodiscard]] FInterpCurveVector GetSplineControlPointsInRoadRange(bool bForwardOrderDir = true,
	                                                                ECoordOffsetType OffsetType =
		                                                                ECoordOffsetType::ROADCENTER,
	                                                                float CustomOffsetOnLeft = 0.0f);

	/**
	 * 获取道路构建点信息，用于确定道路连接的Intersection和其构建顺序，后续可能合并到GetRoadEdgePoints
	 * @param OutLocFromIndex 返回道路构建时起始Intersection
	 * @param OutLocEndIndex 返回道路构建时终止Intersection
	 */
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

	/**
	 * 道路Comp全局ID
	 */
	static int32 RoadGlobalIndex;

	/**
	 * 道路构建时起始的交汇路口全局ID，用于指示道路方向
	 */
	int32 StartToIntersectionIndex = INT32_ERROR;
	/**
	 * 道路构建时终止的交汇路口全局ID，用于指示道路方向
	 */
	int32 EndToIntersectionIndex = INT32_ERROR;

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	void RefreshMatsOnDynamicMeshComp();

	UPROPERTY(EditAnywhere)
	UMaterialInterface* Material;

	void InitialMaterials();

	const FString MaterialPath{"/Game/Road/Material/MI/MI_FreewayAsphalt_Road"};

	const FString BackupMaterialPath{"/JIAPCGAidTool/CityGeneratorContent/MI_Road"};
};
