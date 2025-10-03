#include "Misc/AutomationTest.h"
#include "Road/RoadGraphForBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(RoadGraphTest,
                                 "PCGDemo.JIAPCGAidTool.Source.CityGenerator.UnitTestClass.RoadGraphTest",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool DetectResultsElemNum(const TArray<FBlockLinkInfo>& InLoopsArray)
{
	for (const auto& Loop : InLoopsArray)
	{
		if (Loop.RoadIndexes.Num() != Loop.IntersectionIndexes.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("Unmatch num Between Vertex %d & Edge %d"), Loop.RoadIndexes.Num(),
			       Loop.IntersectionIndexes.Num());
			return false;
		}
	}
	return true;
}

void PrintResults(const TArray<FBlockLinkInfo>& InLoopsArray)
{
	for (const auto& Loop : InLoopsArray)
	{
		FString PrintLog;
		PrintLog += FString::Printf(TEXT("[%d]"), Loop.IntersectionIndexes.Last());
		for (int i = 0; i < Loop.RoadIndexes.Num(); ++i)
		{
			PrintLog += FString::Printf(TEXT("-(%d)-"), Loop.RoadIndexes[i]);
			PrintLog += FString::Printf(TEXT("[%d]"), Loop.IntersectionIndexes[i]);
		}
		UE_LOG(LogTemp, Display, TEXT("%s"), *PrintLog);
	}
}

bool RoadGraphTest::RunTest(const FString& Parameters)
{
	URoadGraph* Graph = NewObject<URoadGraph>();
	//逆时针单边添加
	//测试用例1
	UE_LOG(LogTemp, Display, TEXT("________________________Case1________________________"))
	Graph->AddEdge(0, 3, 3);
	Graph->AddEdge(0, 1, 0);
	Graph->AddEdge(1, 0, 0);
	Graph->AddEdge(1, 3, 4);
	Graph->AddEdge(1, 2, 1);
	Graph->AddEdge(1, 5, 8);
	Graph->AddEdge(1, 4, 5);
	Graph->AddEdge(4, 1, 5);
	Graph->AddEdge(4, 5, 6);
	Graph->AddEdge(3, 2, 2);
	Graph->AddEdge(3, 1, 4);
	Graph->AddEdge(3, 0, 3);
	Graph->AddEdge(2, 5, 7);
	Graph->AddEdge(2, 1, 1);
	Graph->AddEdge(2, 3, 2);
	Graph->AddEdge(5, 4, 6);
	Graph->AddEdge(5, 1, 8);
	Graph->AddEdge(5, 2, 7);
	TArray<FBlockLinkInfo> Loops = Graph->GetSurfaceInGraph();
	if (!DetectResultsElemNum(Loops))
	{
		AddError("Results Has Error Num");
		return false;
	}
	PrintResults(Loops);
	Graph->RemoveAllEdges();
	UE_LOG(LogTemp, Display, TEXT("________________________Case2________________________"))
	//测试用例2
	Graph->AddEdge(0, 4, 4);
	Graph->AddEdge(0, 5, 5);
	Graph->AddEdge(0, 1, 0);
	Graph->AddEdge(1, 0, 0);
	Graph->AddEdge(1, 5, 6);
	Graph->AddEdge(1, 2, 1);
	Graph->AddEdge(2, 1, 1);
	Graph->AddEdge(2, 6, 9);
	Graph->AddEdge(2, 3, 2);
	Graph->AddEdge(3, 2, 2);
	Graph->AddEdge(3, 6, 10);
	Graph->AddEdge(3, 4, 3);
	Graph->AddEdge(4, 3, 3);
	Graph->AddEdge(4, 5, 7);
	Graph->AddEdge(4, 0, 4);
	Graph->AddEdge(5, 6, 8);
	Graph->AddEdge(5, 1, 6);
	Graph->AddEdge(5, 0, 5);
	Graph->AddEdge(5, 4, 7);
	Graph->AddEdge(6, 3, 10);
	Graph->AddEdge(6, 2, 9);
	Graph->AddEdge(6, 5, 8);
	Loops = Graph->GetSurfaceInGraph();
	Graph->RemoveAllEdges();
	if (!DetectResultsElemNum(Loops))
	{
		AddError("Results Has Error Num");
		return false;
	}
	PrintResults(Loops);
	Graph = nullptr;
	return true;
}
