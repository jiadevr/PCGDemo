// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "GenericQuadTree.h"

#include "RoadGeneratorSubsystem.generated.h"

class USplineComponent;


USTRUCT()
struct FSplinePolyLineSegment
{
	GENERATED_BODY()
	FSplinePolyLineSegment()
	{
		//SegmentGlobalIndex++;
	}

public:
	FSplinePolyLineSegment(TWeakObjectPtr<USplineComponent> InSplineRef, int32 InSegmentIndex, int32 InLastSegmentIndex,
	                       const FTransform& InStartTransform,
	                       const FTransform& InEndTransform): OwnerSpline(InSplineRef),
	                                                          SegmentIndex(InSegmentIndex),
	                                                          LastSegmentIndex(InLastSegmentIndex),
	                                                          StartTransform(InStartTransform),
	                                                          EndTransform(InEndTransform)
	{
		GlobalIndex = SegmentGlobalIndex++;
	};

	~FSplinePolyLineSegment()
	{
		OwnerSpline = nullptr;
	}

	UPROPERTY()
	TWeakObjectPtr<USplineComponent> OwnerSpline = nullptr;

	int32 SegmentIndex = 0;
	//这个值是为了排除ClosedLoop最后一点和第一点连接的情况，这边是为了多线程可以不访问Spline对象额外记录的
	int32 LastSegmentIndex = 0;

	FTransform StartTransform = FTransform::Identity;

	FTransform EndTransform = FTransform::Identity;

	uint32 GetGlobalIndex() const { return GlobalIndex; }

protected:
	static uint32 SegmentGlobalIndex;
	uint32 GlobalIndex = 0;
};

USTRUCT()
struct FIntersectionResult
{
	GENERATED_BODY()
	FIntersectionResult()
	{
	}

	FIntersectionResult(const TArray<TWeakObjectPtr<USplineComponent>>& InIntersectedSplines,
	                    const TArray<int32>& InIntersectedSegmentIndex,
	                    const FVector& InIntersectionPoint): IntersectedSplines(
		                                                          InIntersectedSplines),
	                                                          IntersectedSegmentIndex(InIntersectedSegmentIndex),
	                                                          IntersectionPoint(InIntersectionPoint)
	{
	}

	~FIntersectionResult()
	{
		IntersectedSplines.Empty();
	}

	UPROPERTY()
	TArray<TWeakObjectPtr<USplineComponent>> IntersectedSplines;
	TArray<int32> IntersectedSegmentIndex;
	FVector IntersectionPoint = FVector::Zero();
};

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
		return Rectangle2DCoords;
	}
};


UCLASS()
class CITYGENERATOR_API URoadGeneratorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

#pragma region GenerateIntersection

public:
	/**
	 * 初始化样条信息，从CityGeneratorSubsystem中获取有效的Spline信息，调用UpdateSplineSegments把样条转换为芬顿数据
	 * @return 成功获取至少一条样条返回true
	 */
	UFUNCTION(BlueprintCallable)
	bool InitialRoadSplines();
	/**
	 * 更新单根样条的分段数据，**后续可能会对LinearType控制点进行进一步优化**
	 * @param TargetSpline 需要更新数据的样条
	 * @param SampleDistance SplineToPolyLine细分采样数据
	 */
	void UpdateSplineSegments(USplineComponent* TargetSpline, float SampleDistance = 250);

	/**
	 * 根据SplineSegmentsInfo数据调用Get2DIntersection计算样条交点，使用四叉树和备忘录剪枝。
	 * 后续可能会放在多线程做，因此保留返回值形式
	 * @return 返回交点信息
	 */
	//UFUNCTION(BlueprintCallable)
	[[nodiscard]] TArray<FIntersectionResult> FindAllIntersections();

	/**
	* 四叉树网格计算最小网格边长
	*/
	const float MinimumQuadSize = 100.f;


	/**
	 * 交点合并阈值，此范围内的交点会被合并为同一点
	 */
	const float MergeThreshold = 200.0f;

protected:
	bool bNeedRefreshSegmentData = true;
	/**
	 * 绑定OnComponentTransformChanged()，当SplineSegmentsInfo内的Spline移动时，设置bNeedRefreshSegmentData=true,需要重新刷新分段信息
	 * @param MovedComp GEditor回调返回的Component信息
	 * @param MoveType 未使用
	 */
	void OnLevelComponentMoved(USceneComponent* MovedComp, ETeleportType MoveType);
	/**
	 * 记录样条、样条分段数据
	 */
	TMap<TWeakObjectPtr<USplineComponent>, TArray<FSplinePolyLineSegment>> SplineSegmentsInfo;

	/**
	 * 用于加速样条交点计算的四叉树
	 */
	TQuadTree<FSplinePolyLineSegment> SplineQuadTree{FBox2D()};


	/**
	 * 使用参数法计算传入的直线段SegmentA和SegmentB在Start-End范围内的交点，使用快速排斥算法剪枝
	 * @param InSegmentAStart 待测试直线段A起点
	 * @param InSegmentAEnd 待测试直线段A终点
	 * @param InSegmentBStart 待测试直线段B起点
	 * @param InSegmentBEnd 待测试直线段B终点
	 * @param OutIntersection 交点（如存在）
	 * @return SegmentA和SegmentB在Start-End范围内是否存在交点
	 */
	bool Get2DIntersection(const FVector2D& InSegmentAStart, const FVector2D& InSegmentAEnd,
	                       const FVector2D& InSegmentBStart, const FVector2D& InSegmentBEnd,
	                       FVector2D& OutIntersection);


	/**
	 * 使用贝塞尔样条拟合、使用Newton-Raphson法计算两条样条线的交点
	 * @param TargetSplineA 待测试样条A
	 * @param TargetSplineB 待测试样条B
	 * @param IntersectionsIn2DSpace 交点数组
	 * @return 待测试样条A、B在样条全长内是否存在交点
	 */
	bool Get2DIntersection(USplineComponent* TargetSplineA, USplineComponent* TargetSplineB,
	                       TArray<FVector2D>& IntersectionsIn2DSpace);

	TArray<FIntersectionResult> RoadIntersections;

#pragma endregion GenerateIntersection

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
	TMap<TWeakObjectPtr<USplineComponent>, TWeakObjectPtr<AActor>> SplineToMesh;

	UFUNCTION(BlueprintCallable)
	void GenerateRoadInterSection(TArray<USplineComponent*> TargetSplines, float RoadWidth = 400.0f);


	FVector CalculateTangentPoint(const FVector& Intersection, const FVector& EdgePoint);

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
