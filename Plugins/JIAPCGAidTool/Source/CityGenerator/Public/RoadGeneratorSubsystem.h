// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "RoadGeneratorSubsystem.generated.h"

class USplineComponent;
/**
 * 
 */
//道路名称枚举
UENUM()
enum class ELaneType:uint8
{
	SingleWay,
	TwoLaneTwoWay,
	MAX
};

//道路数据结构体
USTRUCT()
struct FLaneMeshInfo
{
	GENERATED_BODY()

public:
	FLaneMeshInfo()
	{
		CrossSectionCoord = FLaneMeshInfo::GetRectangle2DCoords(400.0, 20.0);
	}

	FLaneMeshInfo(const float CrossSectionWidth, const float CrossSectionHeight, const float Length = 500.0f)
	{
		CrossSectionCoord = FLaneMeshInfo::GetRectangle2DCoords(CrossSectionWidth, CrossSectionHeight);
		SampleLength = Length;
	}

	TArray<FVector2D> CrossSectionCoord;
	float SampleLength = 500.0;

protected:
	static TArray<FVector2D> GetRectangle2DCoords(float Width, float Height, bool Clockwise = true)
	{
		TArray<FVector2D> Rectangle2DCoords;
		Rectangle2DCoords.SetNum(4);
		TArray<FVector2D> UnitShape{{0.5, 0.5}, {0.5, -0.5}, {-0.5, -0.5}, {-0.5, 0.5}};
		for (int i = 0; i < UnitShape.Num(); ++i)
		{
			Rectangle2DCoords[i] = FVector2D(Width, Height) * UnitShape[i];
		}
		return MoveTemp(Rectangle2DCoords);
	}
};

UCLASS()
class CITYGENERATOR_API URoadGeneratorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
#pragma region GenerateRoad

public:
	/**
	 * 根据样条生成扫描DynamicMeshActor,挂载DynamicMeshComp和RoadDataComp
	 * @param TargetSpline 目标样条线
	 * @param LaneTypeEnum 车道种类枚举值，目前在本类构造中初始化
	 * @param StartShrink 起始点偏移值（>=0）,生成Mesh由原本0起点偏移值给定长度
	 * @param EndShrink 终点偏移值（>=0）,生成Mesh由原本Last终点偏移值给定长度
	 */
	UFUNCTION(BlueprintCallable)
	void GenerateSingleRoadBySweep(USplineComponent* TargetSpline,
	                               const ELaneType LaneTypeEnum = ELaneType::SingleWay, float StartShrink = 0.0f,
	                               float EndShrink = 0.0f);

	float GetSplineSegmentLength(const USplineComponent* TargetSpline, int32 StartPointIndex);

	/**
	 * 对给定样条每一段进行重采样并整合输出，以获得Sweep所需的路径点。
	 * 当存在Shrink时对开头、结尾、终点进行分别处理，整合算法为起点+开头开区间+中段左闭右开区间+(n-1)+结尾双开区间+终点;Linear需要每一段都为闭区间
	 * @param TargetSpline 目标样条线
	 * @param OutResampledTransform 输出，重采样获得的Transform，非附加方式，传入后会清空再填入数据
	 * @param MaxResampleDistance 最大采样距离
	 * @param StartShrink 起始点偏移值（>=0）,生成Mesh由原本0起点偏移值给定长度
	 * @param EndShrink 终点偏移值（>=0）,生成Mesh由原本Last终点偏移值给定长度
	 * @return 
	 */
	bool ResampleSamplePoint(const USplineComponent* TargetSpline, TArray<FTransform>& OutResampledTransform,
	                         float MaxResampleDistance, float StartShrink = 0.0,
	                         float EndShrink = 0.0);

	TArray<FVector> IntersectionLocation;
	TMap<TWeakObjectPtr<USplineComponent>,TWeakObjectPtr<AActor>>SplineToMesh;
	
	UFUNCTION(BlueprintCallable)
	void GenerateRoadInterSection(TArray<USplineComponent*> TargetSplines,float RoadWidth=400.0f);

	FVector CalculateTangentPoint(const FVector& Intersection,const FVector& EdgePoint);

protected:
	/**
	 * 道路枚举名称-道路构建信息表，在本类的Init中初始化
	 */
	UPROPERTY()
	TMap<ELaneType, FLaneMeshInfo> RoadPresetMap;

	/**
	 * 当具有ShrinkStart、ShrinkEnd时使用该函数，传入使用的下一个节点，返回插值结果，使用TArray不是直接设置点，为了更安全使用返回值形式
	 * @param TargetSpline 目标样条线
	 * @param TargetLength ShinkValue，即ShrinkStartPoint到Index0的距离和ShrinkEndPoint到IndexLast的距离
	 * @param NeighborIndex 与ShrinkPoint相邻的最近有效点序号，对于ShrinkStartPoint应该传入下一个点，对于ShrinkEndPoint应该传入上一点
	 * @param bIsBackTraverse 区分ShrinkStart和ShrinkEnd，ShrinkStart是正向遍历，此处应当传入false；ShrinkEnd是反向遍历，此处应当传入true
	 * @param MaxResampleDistance 最大采样距离，后续还会有调整
	 * @param bIsClosedInterval 是否需要闭合区间，当选择闭合区间时返回带有两端点[ShrinkStart,NextPoint],[PreviousPoint,ShrinkEnd]
	 * @return 返回该Segment插值之后的Transform数组
	 */
	TArray<FTransform> GetSubdivisionBetweenGivenAndControlPoint(const USplineComponent* TargetSpline,
	                                                             float TargetLength, int32 NeighborIndex,
	                                                             bool bIsBackTraverse, float MaxResampleDistance,
	                                                             bool bIsClosedInterval);

	/**
	 * 当仅有两个控制点时生成直线或曲线细分点，如果为闭合样条则获取往复点
	 * @param TargetSpline 目标样条线
	 * @param StartShrink 起始点偏移量（>=0）
	 * @param EndShrink 终点偏移量（>=0）
	 * @param MaxResampleDistance 最大采样距离
	 * @param bIsClosedInterval 是否需要闭合区间，当选择闭合区间时返回带有两端点[ShrinkStart,NextPoint],[PreviousPoint,ShrinkEnd]
	 * @return 返回该Segment插值之后的Transform数组
	 */
	TArray<FTransform> GetSubdivisionOnSingleSegment(const USplineComponent* TargetSpline, float StartShrink,
	                                                 float EndShrink, float MaxResampleDistance,
	                                                 bool bIsClosedInterval);

#pragma endregion GenerateRoad
};
