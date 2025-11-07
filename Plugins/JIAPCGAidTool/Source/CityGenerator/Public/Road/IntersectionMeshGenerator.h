// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MeshGeneratorInterface.h"
#include "Road/RoadSegmentStruct.h"
#include "Components/ActorComponent.h"
#include "IntersectionMeshGenerator.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CITYGENERATOR_API UIntersectionMeshGenerator : public UActorComponent, public IMeshGeneratorInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UIntersectionMeshGenerator();

	/**
	 * 对外接口，设置交点数据
	 * @param InIntersectionData 
	 */
	void SetIntersectionSegmentsData(const TArray<FIntersectionSegment>& InIntersectionData);

	[[nodiscard]] TArray<FIntersectionSegment> GetIntersectionSegmentsData() { return IntersectionsData; }

	/**
	 * 返回交点对象的2D位置盒，被RoadGeneratorSubsystem::GetInteractionOccupiedSegments计算道路切割
	 * @return 返回交叉路口的包围盒
	 */
	FBox2D GetOccupiedBox() const
	{
		return OccupiedBox;
	}

	TArray<FIntersectionSegment> GetRoadConnectionPoint(const TWeakObjectPtr<USplineComponent> InOwnerSpline);

	/**
	 * IMeshGeneratorInterface接口，根据类内的IntersectionsData数据调用同一Owner下挂载的UDynamicMeshComponent创建路口Mesh
	 * @return 返回创建Mesh是否成功 
	 */
	virtual bool GenerateMesh() override;
	/**
	 * IMeshGeneratorInterface接口，设置Owner的UDynamicMeshComponent，变量声明在接口中
	 * @param InMeshComponent 传入Owner的UDynamicMeshComponent
	 */
	virtual void SetMeshComponent(class UDynamicMeshComponent* InMeshComponent) override;

	//这个函数有问题，看后续还要不要维护
	//int32 GetOverlapSegmentOnGivenSpline(TWeakObjectPtr<USplineComponent> TargetSpline);


	/**
	 * 获取从给定EntryIndex到顺时针下一个Entry的过渡点数组
	 * @param EntryIndex EntryIndex,以该点为起点
	 * @return 到下一个Entry的过渡点的数组
	 */
	TArray<FVector> GetTransitionalPoints(int32 EntryIndex, bool bOpenInterval = true);

	/**
	 * Debug函数，配合TargetEntryIndex使用，用于在街区生成时绘制对应组的过渡点进行Debug，
	 * 根据设计当以TargetEntryIndex对应衔接点朝向交汇点时，显示点应当在面向方向左侧
	 */
	UFUNCTION(CallInEditor)
	void DrawTransitionalPoints();

	/**
	 * Debug变量，配合DrawTransitionalPoints使用，对应需要测试的RoadEntryIndex
	 */
	UPROPERTY(EditInstanceOnly)
	int32 TargetEntryIndex = 0;

protected:
	/**
	 * 核心函数，创建交点二维截面
	 * @return 逆时针顺序二维截面坐标数组
	 */
	[[nodiscard]] TArray<FVector2D> CreateExtrudeShape();

	TArray<FVector2D> ExtrudeShape;

	/**
	 * 两个路口衔接点直接的过渡段细分数目
	 */
	int32 TransitionalSubdivisionNum = 10;
	/**
	 * 计算模拟Spline的Tangent值，对于每个端点都需要计算一次
	 * @param Intersection Spline的Segment交点，Spline不会经过该点
	 * @param EdgePoint Segment起点、终点
	 * @return Tangent值
	 */
	FVector2D CalTransitionalTangentOnEdge(const FVector2D& Intersection, const FVector2D& EdgePoint);

	/**
	 * 当前交汇路口占据的空间位置，用于二叉树查找
	 */
	FBox2D OccupiedBox;
	/**
	 * 包含信息见结构体RoadSegmentStruct.h
	 */
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly)
	TArray<FIntersectionSegment> IntersectionsData;

	/**
	 * 回报给道路构建系统的衔接点，使用Map保存会导致值无序
	 */
	TMultiMap<TWeakObjectPtr<USplineComponent>, FIntersectionSegment> ConnectionLocations;

	/**
	 * 交汇路口全局ID
	 */
	static int32 IntersectionGlobalIndex;


	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	void RefreshMatsOnDynamicMeshComp();

	UPROPERTY()
	UMaterialInterface* Material;

	void InitialMaterials();


	const FString MaterialPath{"/Game/Road/Material/MI/M_Asphalt_Master_Inst_Intersection"};

	const FString BackupMaterialPath{"/JIAPCGAidTool/CityGeneratorContent/Materials/MI_Intersection"};
};
