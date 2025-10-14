// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "GenericQuadTree.h"
#include "RoadGraphForBlock.h"
#include "Road/RoadSegmentStruct.h"
#include "RoadGeneratorSubsystem.generated.h"

class UBlockMeshGenerator;
class URoadMeshGenerator;
class UIntersectionMeshGenerator;
class USplineComponent;

/**
 * 交点插入到连续分段的信息
 */
struct FConnectionInsertInfo
{
	FConnectionInsertInfo()
	{
	};
	/**
	 * 插入到二维数组ContinuousSegmentsGroups中一维的哪一个元素，即作为哪个连续SegmentsGroup的头或尾
	 */
	int32 GroupIndex = -1;
	/**
	 * 插入到连续SegmentsGroup的头部（true）或尾部（false），同时决定了插入位置，头部一定在原有元素之前，尾部一定在原有元素之后
	 */
	bool bConnectToGroupHead = true;
	/**
	* 连接点Transform
	*/
	FTransform ConnectionTrans;

	/**
	 * 连接的交汇路口全局ID
	 */
	int32 IntersectionGlobalIndex = INT32_ERROR;

	/**
	 * 连接的交汇路口的具体入口局部ID
	 */
	int32 EntryLocalIndex = INT32_ERROR;
};


/**
 * 该类主要实现以下内容：
 * 1.承接CityGenerator类中用户输入的Spline信息，将其进行细分分段并转换为PolyLineSegment用于计算交点
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
	 * 绑定OnWorldDestroyed()，当切换World的时候调用
	 * @param World 
	 */
	void OnWorldChanged(UWorld* World);

	FDelegateHandle WorldChangeDelegate;

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
	void VisualizeSegmentByDebugline(bool bUpdateBeforeDraw = false, float Thickness = 30.0f,bool bFlushBeforeDraw=false);

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
	                                TArray<FIntersectionSegment>& OutSegments, float UniformDistance = 1000.0f);

	TMap<TWeakObjectPtr<USplineComponent>, TSet<TWeakObjectPtr<UIntersectionMeshGenerator>>> IntersectionCompOnSpline;

#pragma endregion GenerateIntersection

#pragma region GenerateRoad

public:
	/**
	 * 对外接口，需要在交叉路口生成之后调用，生成道路构建信息具体功能包括：
	 * 1. 从SplineSegmentsInfo中获取样条信息并于十字路口的内容计算相交，获得**连续的Segments**作为道路基础信息
	 * 2. 获取十字路口的端点，判断端点位于哪个Segment、将其作为附加信息与连续Segments封装到FConnectionInsertInfo结构体
	 * 3. 生成道路Actor，配置基础信息并将FConnectionInsertInfo结构体发送给道路Actor挂载的URoadMeshGenerator
	 * 4. 向路网有向图中添加节点和边
	 * 5，调用道路生成
	 */
	UFUNCTION(BlueprintCallable)
	void GenerateRoads();

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
	 * 在已有的连续数组中找到给定点所属的Segment信息，用于确定衔接到路口的点应当作为哪一个连续SegmentsGroup的附属数据以及应当插入的位置
	 * 具体数据插入位于RoadMeshGenerator，请勿使用该结果在本类中进行数据插入，Global新增会影响分割结果
	 * @param InContinuousSegmentsGroups 已经分割的RoadSegmentIndex数组
	 * @param InAllSegmentOnSpline 所在样条上的所有Segments，用于查找交点所在Segment
	 * @param OwnerSegmentID 交点所在的Segment
	 * @param PointTransWS 焦点位置
	 * @return 
	 */
	FConnectionInsertInfo FindInsertIndexInExistedContinuousSegments(
		const TArray<TArray<uint32>>& InContinuousSegmentsGroups,
		const TArray<FSplinePolyLineSegment>&
		InAllSegmentOnSpline,
		const uint32 OwnerSegmentID,
		const FVector& PointTransWS);

#pragma endregion GenerateRoad


#pragma region GenerateBlock

protected:
	/**
	 * Debug函数，为Actor添加TextRenderComponent用于显示给定Debug信息
	 * @param TargetActor 添加目标Actor
	 * @param TextColor TextRender颜色
	 * @param Text TextRender显示文字
	 */
	void AddDebugTextRender(AActor* TargetActor, const FColor& TextColor, const FString& Text);

	//EidtorSubsystem启动顺序靠前，不能直接成员类
	UPROPERTY()
	URoadGraph* RoadGraph = nullptr;

	/**
	 * 道路全局ID-道路Generator表
	 */
	UPROPERTY()
	TMap<int32, TWeakObjectPtr<URoadMeshGenerator>> IDToRoadGenerator;

	/**
	 * 交汇路口全局ID-交汇路口Generator表
	 */
	UPROPERTY()
	TMap<int32, TWeakObjectPtr<UIntersectionMeshGenerator>> IDToIntersectionGenerator;

	/**
	 * 街区全局ID-街区Generator表
	 */
	UPROPERTY()
	TMap<int32, TWeakObjectPtr<UBlockMeshGenerator>> IDToBlockGenerator;

	/**
	 * 删除外轮廓，需要ComponentOwner位置信息
	 * @param OutBlockLoops 原地修改环、面信息
	 */
	void RemoveInvalidLoopInline(TArray<FBlockLinkInfo>& OutBlockLoops);

public:
	/**
	 * 对外接口，需要在交汇路口和道路生成之后调用，生成由道路和交汇路口围成的城区几何体具体功能包括：
	 * 1. 从图中获取环/插入面的信息，提供数据提取依据
	 * 2. 从URoadMeshGenerator中提取道路边线信息、从UIntersectionMeshGenerator中提取路口过渡段信息
	 * 3. 生成街区Actor，配置基础信息并将上面提取到的新信息发送给UBlockMeshGenerator
	 * 4. 调用街区生成
	 */
	UFUNCTION(BlueprintCallable)
	void GenerateCityBlock();


	/**
	 * Debug函数，用于打印路网邻接表
	 */
	UFUNCTION(BlueprintCallable)
	void PrintGraphConnection();

protected:
#pragma endregion GenerateBlock

protected:
	bool IsIntegerInFloatFormat(float InFloatValue);
};

/**
 * 根据InsertMap中配置的Index和数组，在给定Index **之后** 按照顺序插入数组元素，此函数用于一次性完成整段数组元素插入，提升多处插入的性能
 * @tparam T 元素类型，支持非POD对象（Transform等）
 * @param TargetArray 待插入数组，原位插入
 * @param InsertMap 插入元素表，其中MapKey为待插入位置，将插入到该元素**之后**，MapValue为待插入元素值
 */
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


