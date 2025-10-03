// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RoadGraphForBlock.generated.h"

USTRUCT()
struct FBlockLinkInfo
{
	GENERATED_BODY()
	TArray<int32> RoadIndexes;
	TArray<int32> IntersectionIndexes;
};

/**
 * 使用交汇口作为节点，道路作为边的图结构，使用邻接表实现，为街区生成提供基础数据
 * 提供基于平面嵌入的「最小逆时针环」枚举算法用于计算街区
 * 类内不包含空间信息，不提供空间排序算法，因此在加入边的时候需要注意顺序
 */
UCLASS()
class URoadGraph final : public UObject
{
	GENERATED_BODY()
	friend class URoadGeneratorSubsystem;
	friend class RoadGraphTest;

public:
	URoadGraph()
	{
	}

	virtual ~URoadGraph() override;

protected:
	struct FRoadEdge
	{
		int32 ToNodeIndex;
		int32 RoadIndex;

		FRoadEdge()
		{
			ToNodeIndex = INT32_ERROR;
			RoadIndex = INT32_ERROR;
		}

		FRoadEdge(int32 InToNodeIndex, int32 InRoadIndex) :
			ToNodeIndex(InToNodeIndex), RoadIndex(InRoadIndex)
		{
		};
	};

	/**
	 * 为了利用之前的保序数据提供的特殊接口,直接添加对应边到给定邻接表位置
	 * 由于图中没有保存坐标，无法直接进行空间排序，使用该接口作为输入直接将之前已排序数据放入邻接表
	 * @param FromNodeIndex 起始节点序号
	 * @param ToNodeIndex 终止节点序号
	 * @param EdgeIndex 边序号
	 * @param SlotIndexOfFromNode 要插入的邻接表位置、对应排序后的元素编号 
	 */
	void AddEdgeInGivenSlot(int32 FromNodeIndex, int32 ToNodeIndex, int32 EdgeIndex, int32 SlotIndexOfFromNode);


	/**
	 * 为节点添加有向边，用于TestClass；
	 * 类内不提供空间排序，在插入边时需要按照固定顺序
	 * @param FromNodeIndex 起始节点序号
	 * @param ToNodeIndex 终止节点序号
	 * @param EdgeIndex 边序号
	 */
	void AddEdge(int32 FromNodeIndex, int32 ToNodeIndex, int32 EdgeIndex);

	//DOF-
	/*void AddUndirectedEdge(int32 NodeAIndex, int32 NodeBIndex, int32 EdgeIndex);*/


	/**
	 * 去除给定编号的单向边，当RoadIndex保持默认时去除FromNode到ToNode中所有单向边；移除将内容置为无效值，避免引起图顺序变化
	 * @param FromNodeIndex 起始节点序号
	 * @param ToNodeIndex 终止节点序号
	 * @param RoadIndex 道路编号，当RoadIndex保持默认（INT32_ERROR）时去除FromNode到ToNode中所有单向边
	 */
	void RemoveEdge(int32 FromNodeIndex, int32 ToNodeIndex, int32 RoadIndex = INT32_ERROR);

	/**
	 * 移除所有边
	 */
	void RemoveAllEdges();

	/**
	 * 判定两个节点直接存在单向边
	 * @param FromNodeIndex 起始节点序号
	 * @param ToNodeIndex 终止节点序号
	 * @return 是否包含从From到To的单向边
	 */
	bool HasEdge(int32 FromNodeIndex, int32 ToNodeIndex) const;

	/**
	 * 返回两个节点中第一个单向边的序号，不存在时返回INT32_ERROR
	 * @param FromNodeIndex 起始节点序号
	 * @param ToNodeIndex 终止节点序号
	 * @return 第一个单向边的序号
	 */
	int32 GetRoadIndex(int32 FromNodeIndex, int32 ToNodeIndex);

	/**
	 * 打印邻接表，以[FromNodeIndex]-(RoadIndex)-[ToNodeIndex]格式逐条打印
	 */
	void PrintConnectionToLog();

	/**
	 * 支持基于平面嵌入的「最小逆时针环」枚举算法
	 * 给定一个道路网络（路口=顶点，道路=无向边），找出所有被道路完全包围、且内部不再被任何道路横穿的最小面域。
	 * 模拟半边计算图中的插入面，会包括外轮廓边（可以配合点坐标使用Shoelace公式去除）
	 * @return 外轮廓数组，以边开始，首个顶点位于IntersectionIndexes.Last(0)
	 */
	TArray<FBlockLinkInfo> GetSurfaceInGraph();

	/**
	 * 稀疏图，使用邻接表实现
	 */
	TArray<TArray<FRoadEdge>> Graph;

	/**
	 * 返回邻接表中相邻的下一条边，当传入最后一条边时返回首条边
	 * @param NodeIndex 当前节点序号
	 * @param CurrentEdgeIndex 当前边信息
	 * @return 相邻的下一条边
	 */
	FRoadEdge* FindNextEdge(int32 NodeIndex, const FRoadEdge& CurrentEdgeIndex);

	/**
	 * 图中边总计算
	 */
	int32 EdgeCount = 0;

	/**
	 * 使用边的单Index模拟半边，当FromNode.VertexIndex<ToNode.VertexIndex时返回2*RoadIndex，否则返回2*RoadIndex+1；
	 * @param FromNodeIndex 邻接表第一维参数（出发顶点）
	 * @param ToNodeIndex 邻接表第二维中到达顶点参数
	 * @param RoadIndex 邻接表第二维中的道路编号
	 * @return 一维有向边序号
	 */
	int32 GetDirectionalEdgeIndex(int32 FromNodeIndex, int32 ToNodeIndex, int32 RoadIndex);
};
