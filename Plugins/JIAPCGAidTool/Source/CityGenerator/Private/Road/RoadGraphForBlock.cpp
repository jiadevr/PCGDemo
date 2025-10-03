// Fill out your copyright notice in the Description page of Project Settings.


#include "Road/RoadGraphForBlock.h"


URoadGraph::~URoadGraph()
{
	Graph.Empty();
}

void URoadGraph::AddEdge(int32 FromNodeIndex, int32 ToNodeIndex, int32 EdgeIndex)
{
	if (FromNodeIndex == INT32_ERROR || ToNodeIndex == INT32_ERROR)
	{
		return;
	}

	if (!Graph.IsValidIndex(FromNodeIndex))
	{
		if (FromNodeIndex > Graph.Num() - 1)
		{
			Graph.SetNum(FromNodeIndex + 1);
		}
	}
	Graph[FromNodeIndex].Emplace(FRoadEdge(ToNodeIndex, EdgeIndex));
	EdgeCount++;
}

void URoadGraph::AddEdgeInGivenSlot(int32 FromNodeIndex, int32 ToNodeIndex, int32 EdgeIndex, int32 SlotIndexOfFromNode)
{
	if (FromNodeIndex == INT32_ERROR || ToNodeIndex == INT32_ERROR || SlotIndexOfFromNode < 0)
	{
		return;
	}

	if (!Graph.IsValidIndex(FromNodeIndex))
	{
		if (FromNodeIndex > Graph.Num() - 1)
		{
			Graph.SetNum(FromNodeIndex + 1);
		}
	}
	if (!Graph[FromNodeIndex].IsValidIndex(SlotIndexOfFromNode))
	{
		Graph[FromNodeIndex].SetNum(SlotIndexOfFromNode + 1);
	}
	Graph[FromNodeIndex][SlotIndexOfFromNode] = FRoadEdge(ToNodeIndex, EdgeIndex);
	EdgeCount++;
}

void URoadGraph::RemoveEdge(int32 FromNodeIndex, int32 ToNodeIndex, int32 RoadIndex)
{
	TArray<FRoadEdge>& AllConnectedNodes = Graph[FromNodeIndex];
	for (int32 i = 0; i < AllConnectedNodes.Num(); ++i)
	{
		if (AllConnectedNodes[i].ToNodeIndex == ToNodeIndex)
		{
			if (RoadIndex == INT32_ERROR)
			{
				AllConnectedNodes[i].ToNodeIndex = INT32_ERROR;
				AllConnectedNodes[i].RoadIndex = INT32_ERROR;
				EdgeCount--;
			}
			else if (AllConnectedNodes[i].RoadIndex == RoadIndex)
			{
				AllConnectedNodes[i].ToNodeIndex = INT32_ERROR;
				AllConnectedNodes[i].RoadIndex = INT32_ERROR;
				EdgeCount--;
				break;
			}
		}
	}
	bool bAllInvalid = true;
	for (int32 i = 0; i < AllConnectedNodes.Num(); ++i)
	{
		bAllInvalid &= AllConnectedNodes[i].ToNodeIndex == INT32_ERROR;
	}
	if (bAllInvalid)
	{
		AllConnectedNodes.Empty();
	}
}

void URoadGraph::RemoveAllEdges()
{
	for (TArray<FRoadEdge>& NeighborOfNode : Graph)
	{
		NeighborOfNode.Empty();
	}
}

bool URoadGraph::HasEdge(int32 FromNodeIndex, int32 ToNodeIndex) const
{
	const TArray<FRoadEdge>& AllConnectedNodes = Graph[FromNodeIndex];
	for (int32 i = 0; i < AllConnectedNodes.Num(); ++i)
	{
		if (AllConnectedNodes[i].ToNodeIndex == ToNodeIndex && AllConnectedNodes[i].RoadIndex != INT32_ERROR)
		{
			return true;
		}
	}
	return false;
}

int32 URoadGraph::GetRoadIndex(int32 FromNodeIndex, int32 ToNodeIndex)
{
	int32 RoadIndex = INT32_ERROR;
	if (HasEdge(FromNodeIndex, ToNodeIndex))
	{
		RoadIndex = Graph[FromNodeIndex][ToNodeIndex].RoadIndex;
	}
	return RoadIndex;
}

void URoadGraph::PrintConnectionToLog()
{
	if (Graph.IsEmpty())
	{
		UE_LOG(LogTemp, Display, TEXT("Graph is empty"));
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("Begin To Print Graph Connections"));
	for (int32 i = 0; i < Graph.Num(); i++)
	{
		for (int j = 0; j < Graph[i].Num(); ++j)
		{
			int32 ToNode = Graph[i][j].ToNodeIndex;
			int32 ByEdge = Graph[i][j].RoadIndex;
			FString SinglePath = FString::Printf(TEXT("[%d]-(%d)-[%d],"), i, ByEdge, ToNode);
			UE_LOG(LogTemp, Display, TEXT("%s"), *SinglePath);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("Print Graph Connections Finished"));
}

TArray<FBlockLinkInfo> URoadGraph::GetSurfaceInGraph()
{
	TArray<FBlockLinkInfo> Results;
	if (Graph.IsEmpty())
	{
		return Results;
	}
	
	//二维表转一维表用于和bVisited形成对应；但因为RoadIndex可以删除数组可能不连贯
	//不能直接使用RoadIndex作为数组ID，可能开头不为0、可能不连贯、双向无法区分
	//设计一个编码算法节点A->B，当A<B时+0；当A>B时+最大跨度(MaxRoadIndex)，为了能拿到节点需要从节点开始遍历
	TArray<FRoadEdge*> AllEdges;
	TMap<int32, bool> bVisited;
	bVisited.Reserve(EdgeCount);
	//因为边数据中没有保存FromVertex，利用数组有序特性先记录这个值
	int32 FromVertex = INT32_ERROR;
	for (int32 i = 0; i < Graph.Num(); ++i)
	{
		for (auto& Edge : Graph[i])
		{
			if (FromVertex == INT32_ERROR)
			{
				FromVertex = i;
			}
			bVisited.Emplace(GetDirectionalEdgeIndex(i, Edge.ToNodeIndex, Edge.RoadIndex), false);
			AllEdges.Emplace(&Edge);
		}
	}
	//DFS搜索，备忘录剪枝
	for (int i = 0; i < AllEdges.Num(); ++i)
	{
		int32 TargetEdgeIndex = GetDirectionalEdgeIndex(FromVertex, AllEdges[i]->ToNodeIndex, AllEdges[i]->RoadIndex);
		if (bVisited[TargetEdgeIndex])
		{
			continue;
		}
		bVisited[TargetEdgeIndex] = true;
		FBlockLinkInfo Surface;
		Surface.RoadIndexes.Emplace(AllEdges[i]->RoadIndex);
		const FRoadEdge* CurrentRoad = AllEdges[i];
		const FRoadEdge* NextRoad = nullptr;
		while (NextRoad != AllEdges[i])
		{
			if (nullptr != NextRoad)
			{
				Surface.RoadIndexes.Emplace(NextRoad->RoadIndex);
				TargetEdgeIndex = GetDirectionalEdgeIndex(FromVertex, NextRoad->ToNodeIndex, NextRoad->RoadIndex);
				bVisited[TargetEdgeIndex] = true;
			}
			int32 NextVertex = CurrentRoad->ToNodeIndex;
			Surface.IntersectionIndexes.Emplace(NextVertex);
			NextRoad = FindNextEdge(NextVertex, *CurrentRoad);
			CurrentRoad = NextRoad;
			FromVertex = NextVertex;
		}
		if (Surface.RoadIndexes.Num() >= 2)
		{
			Results.Emplace(Surface);
		}
	}
	return Results;
}


URoadGraph::FRoadEdge* URoadGraph::FindNextEdge(int32 NodeIndex, const FRoadEdge& CurrentEdgeIndex)
{
	const int32 NeighboursCount = Graph[NodeIndex].Num();
	int32 i = 0;
	for (; i < NeighboursCount; ++i)
	{
		if (Graph[NodeIndex][i].RoadIndex == CurrentEdgeIndex.RoadIndex)
		{
			break;
		}
	}
	return &Graph[NodeIndex][(i + 1) % NeighboursCount];
}

int32 URoadGraph::GetDirectionalEdgeIndex(int32 FromNodeIndex, int32 ToNodeIndex, int32 RoadIndex)
{
	if (FromNodeIndex < ToNodeIndex)
	{
		return 2 * RoadIndex;
	}
	return 2 * RoadIndex + 1;
}
