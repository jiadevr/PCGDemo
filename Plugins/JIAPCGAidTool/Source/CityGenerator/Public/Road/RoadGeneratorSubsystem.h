// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "GenericQuadTree.h"
#include "Road/RoadSegmentStruct.h"

#include "RoadGeneratorSubsystem.generated.h"
class URoadMeshGenerator;
class UIntersectionMeshGenerator;
class USplineComponent;

/**
 * 该类主要实现以下内容：
 * 1.承接CityGenerator类中用户输入的Spline信息，将其转换为PolyLineSegment并计算交点
 * 2.生成RoadActor和IntersectionActor，为其挂载的Generator传递核心Segment信息，由Generator负责结构细化和具体生成
 */

UCLASS()
class CITYGENERATOR_API URoadGeneratorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 初始化数据，绑定SplineComponent移动事件
	 * @param Collection 
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	bool bNeedRefreshSegmentData = true;
	/**
	 * 绑定OnComponentTransformChanged()，当SplineSegmentsInfo内的Spline移动时，设置bNeedRefreshSegmentData=true,需要重新刷新分段信息
	 * @param MovedComp GEditor回调返回的Component信息
	 * @param MoveType 未使用
	 */
	void OnLevelComponentMoved(USceneComponent* MovedComp, ETeleportType MoveType);

	FDelegateHandle ComponentMoveHandle;

	/**
	 * 绑定OnLevelActorDeleted()主要用于解决
	 * @param RemovedActor 
	 */
	void OnRoadActorRemoved(AActor* RemovedActor);

	FDelegateHandle RoadActorRemovedHandle;

	virtual void Deinitialize() override;

#pragma region GenerateIntersection

public:
	/**
	 * 对外接口，选择性更新样条信息、计算交点生成Actor
	 */
	UFUNCTION(BlueprintCallable)
	void GenerateIntersections();

	/**
	* 四叉树网格计算最小网格边长
	*/
	const float MinimumQuadSize = 100.f;

	/**
	 * 交点合并阈值，此范围内的交点会被合并为同一点
	 */
	const float MergeThreshold = 200.0f;

	/**
	 * Debug函数，显示所有Spline的Segment分割情况，会之前会调用FlushPersistentDebugLines清除其他绘制信息
	 * @param bUpdateBeforeDraw 是否首先刷新Spline信息
	 * @param Thickness Debug绘制宽度
	 */
	UFUNCTION(BlueprintCallable)
	void VisualizeSegmentByDebugline(bool bUpdateBeforeDraw = false, float Thickness = 30.0f);
	
	/**
	 * 模板函数，用于ResampleSpline函数中长直线段细分数据加入，以非POD（不能使用FMemoryCopy）为处理对象
	 * 应当为Protected，为了满足单元测试需求设置为Public
	 * @tparam T POD变量类型，在类中用于FTransform
	 * @param TargetArray 原数组，原位改写
	 * @param InsertMap 插入序号——插入数据数组表，插入数据数组为被插入到序号后边
	 */
	template <typename T>
	void InsertElementsAtIndex(TArray<T>& TargetArray, const TMap<int32, TArray<T>>& InsertMap);

protected:
	bool bIntersectionsGenerated = false;
	/**
	* 初始化样条信息，从CityGeneratorSubsystem中获取有效的Spline信息，调用UpdateSplineSegments把样条转换为芬顿数据
	* @return 成功获取至少一条样条返回true
	*/
	UFUNCTION(BlueprintCallable)
	bool InitialRoadSplines();

	TSet<TWeakObjectPtr<USplineComponent>> RoadSplines;

	/**
	 * 更新单根样条的分段数据，**后续可能会对LinearType控制点进行进一步优化**
	 * @param TargetSpline 需要更新数据的样条
	 */
	void UpdateSplineSegments(USplineComponent* TargetSpline);
	
	//这个值50分段大概在1000cm
	const float PolyLineSampleDistance = 200.0f;

	/**
	 * 用于将根据PolyLineSampleDistance值Spline进行细分重采样，
	 * 主体函数是ConvertSplineToPolyLineWithDistances，在此基础上加入了对长直线的细分，避免在交点计算时发生大范围切断导致道路无法连接
	 * @param TargetSpline 目标样条线
	 * @return 重采样的细分点Transform
	 */
	TArray<FTransform> ResampleSpline(const USplineComponent* TargetSpline);

	/**
	 * 根据SplineSegmentsInfo数据调用Get2DIntersection计算样条交点，使用四叉树和备忘录剪枝。
	 * 后续可能会放在多线程做，因此保留返回值形式
	 * @return 返回交点信息
	 */
	//UFUNCTION(BlueprintCallable)
	[[nodiscard]] TArray<FSplineIntersection> FindAllIntersections();
	/**
	 * 记录样条、样条分段数据
	 */
	TMap<TWeakObjectPtr<USplineComponent>, TArray<FSplinePolyLineSegment>> SplineSegmentsInfo;

	/**
	 * 用于加速样条交点计算的四叉树
	 */
	TQuadTree<FSplinePolyLineSegment> SplineQuadTree{FBox2D()};
	
	/**
	 * 使用四叉树返回交点处重叠的Segment信息，用于道路切割
	 * @param TargetIntersection 交点对象引用，需要交点对象的GetOccupiedBox
	 * @return 返回四叉树查询获得的全部Segment信息
	 */
	TArray<FSplinePolyLineSegment> GetInteractionOccupiedSegments(
		TWeakObjectPtr<UIntersectionMeshGenerator> TargetIntersection) const;
	
	/**
	 * 生成的路口Actor上挂载的Component数组
	 */
	TArray<TWeakObjectPtr<UIntersectionMeshGenerator>> RoadIntersectionsComps;

	/**
	  * 根据样条交点产生的单个FSplineIntersection拆分为可以进行生成的基础信息FIntersectionSegment数组
	  * @param InIntersectionInfo 传入交点信息
	  * @param OutSegments 拆分为Segment数组
	  * @param UniformDistance 统一采样距离，后续可能改写
	  * @return 返回是否拆分成功
	  */
	bool TearIntersectionToSegments(const FSplineIntersection& InIntersectionInfo,
	                                TArray<FIntersectionSegment>& OutSegments, float UniformDistance = 500.0f);

	TMap<TWeakObjectPtr<USplineComponent>, TSet<TWeakObjectPtr<UIntersectionMeshGenerator>>> IntersectionCompOnSpline;

#pragma endregion GenerateIntersection

#pragma region GenerateRoad

public:
	/**
	 * 对外接口，需要在交叉路口生成之后调用，生成道路构建信息
	 */
	UFUNCTION(BlueprintCallable)
	void GenerateRoads();
	
	TArray<TWeakObjectPtr<URoadMeshGenerator>> RoadMeshGenerators;
	
	/**
	 * 根据样条生成扫描DynamicMeshActor,挂载DynamicMeshComp和RoadDataComp
	 * @param TargetSpline 目标样条线
	 * @param LaneTypeEnum 车道种类枚举值，目前在本类构造中初始化
	 * @param StartShrink 起始点偏移值（>=0）,生成Mesh由原本0起点偏移值给定长度
	 * @param EndShrink 终点偏移值（>=0）,生成Mesh由原本Last终点偏移值给定长度
	 */
	UFUNCTION(BlueprintCallable)
	void GenerateSingleRoadBySweep(USplineComponent* TargetSpline,
	                               const ELaneType LaneTypeEnum = ELaneType::ARTERIALROADS, float StartShrink = 0.0f,
	                               float EndShrink = 0.0f);

	/**
	 * 获得Spline给定Segment的长度，支持CloseLoop
	 * @param TargetSpline 指定Spine
	 * @param SegmentIndex 需要获取的Segment序号
	 * @return 该Segment长度，当SegmentIndex不合法是返回0
	 */
	float GetSplineSegmentLength(const USplineComponent* TargetSpline, int32 SegmentIndex);

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
	/**
	 * 将传入的连续SegmentIndex（有序）按照BreakPoints(可以无序)切分成多少个连续子数组，子数组不含断点元素，两数组要求元素唯一
	 * 单元测试函数位于FRoadGeneratorSubsystemTest的TestGetContinuousIndexSeries，函数本身应该是Protected，但是为了单元测试放在Public
	 * @param AllSegmentIndex 连续有序的SegmentIndex，要求元素唯一
	 * @param BreakPoints 断点数组，可无序，要求元素唯一
	 * @return 切分获得的子数组（不含断点元素）
	 */
	TArray<TArray<uint32>> GetContinuousIndexSeries(const TArray<uint32>& AllSegmentIndex, TArray<uint32>& BreakPoints);

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
	 * @param bIsLocalSpace 是否返回局部空间坐标
	 * @return 返回该Segment插值之后的Transform数组
	 */
	TArray<FTransform> GetSubdivisionOnSingleSegment(const USplineComponent* TargetSpline, float StartShrink,
	                                                 float EndShrink, float MaxResampleDistance,
	                                                 bool bIsClosedInterval, bool bIsLocalSpace);

#pragma endregion GenerateRoad
};

template <typename T>
void URoadGeneratorSubsystem::InsertElementsAtIndex(TArray<T>& TargetArray, const TMap<int32, TArray<T>>& InsertMap)
{
	if (InsertMap.Num() == 0)
	{
		return;
	}
	TArray<int32> SortedIndexes;
	InsertMap.GetKeys(SortedIndexes);
	SortedIndexes.Sort();

	int32 TotalElementsToInsert = 0;
	for (const auto& Pair : InsertMap)
	{
		TotalElementsToInsert += Pair.Value.Num();
	}
	const int32 FinalSize = TargetArray.Num() + TotalElementsToInsert;
	TargetArray.Reserve(FinalSize);

	TArray<T> TempArray;
	TempArray.SetNum(FinalSize);

	int32 SourceIndex = 0;
	int32 NewIndex = 0;

	for (int32 InsertIndex : SortedIndexes)
	{
		int32 Count = InsertIndex - SourceIndex + 1;
		for (int32 i = 0; i < Count; ++i)
		{
			TempArray[NewIndex++] = TargetArray[SourceIndex++];
		}
		const TArray<T>& InsertElems = InsertMap[InsertIndex];
		for (const T& Elem : InsertElems)
		{
			TempArray[NewIndex++] = Elem;
		}
	}
	while (SourceIndex < TargetArray.Num())
	{
		TempArray[NewIndex++] = TargetArray[SourceIndex++];
	}

	TargetArray = MoveTemp(TempArray);
}
