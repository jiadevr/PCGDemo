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
	 * @param FromEntryIndex EntryIndex,以该点为起点
	 * @return 到下一个Entry的过渡点的数组
	 */
	TArray<FVector2D> GetTransitionalPoints(int32 FromEntryIndex,bool bOpenInterval=true);
protected:
	/**
	 * 核心函数，创建交点二维截面
	 * @return 逆时针顺序二维截面坐标数组
	 */
	[[nodiscard]] TArray<FVector2D> CreateExtrudeShape();

	TArray<FVector2D> ExtrudeShape;

	int32 TransitionalSubdivisionNum=10;
	/**
	 * 计算模拟Spline的Tangent值，对于每个端点都需要计算一次
	 * @param Intersection Spline的Segment交点，Spline不会经过该点
	 * @param EdgePoint Segment起点、终点
	 * @return Tangent值
	 */
	FVector2D CalTransitionalTangentOnEdge(const FVector2D& Intersection, const FVector2D& EdgePoint);

	FBox2D OccupiedBox;
	/**
	 * 包含信息见结构体RoadSegmentStruct.h
	 */
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly)
	TArray<FIntersectionSegment> IntersectionsData;

	TMultiMap<TWeakObjectPtr<USplineComponent>, FIntersectionSegment> ConnectionLocations;

	static int32 IntersectionGlobalIndex;
};
