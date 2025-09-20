#include "Misc/AutomationTest.h"
#include "Road/RoadGeneratorSubsystem.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FRoadGeneratorSubsystemTest,
                                  "PCGDemo.JIAPCGAidTool.Source.CityGenerator.UnitTestClass.FRoadGeneratorSubsystemTest",
                                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)


bool TestGetContinuousIndexSeries(URoadGeneratorSubsystem* Subsystem, TArray<int32>& BreakPoints, int32 StartIndex,
                                  int32 EndIndex)
{
	//输入给定端点数组、起点终点，输出数组
	//数组个数不好确定，但 每一组数组内容应当连续递增&&每组不包含端点元素&&输出数组和**有效**断点元素总数和原输入数组相同
	TArray<uint32> ContinuousIndexSeries;
	FString DebugStr = "Original Array:";
	uint32 EndIndexInU32 = static_cast<uint32>(EndIndex);
	for (uint32 i = StartIndex; i <= EndIndexInU32; ++i)
	{
		ContinuousIndexSeries.Emplace(i);
		DebugStr += FString::Printf(TEXT("%d,"), i);
	}
	UE_LOG(LogTemp, Display, TEXT("%s"), *DebugStr);
	DebugStr.Reset();
	TArray<uint32> BreakpointsInU32;
	BreakpointsInU32.SetNumUninitialized(BreakPoints.Num());
	FMemory::Memcpy(BreakpointsInU32.GetData(), BreakPoints.GetData(), BreakPoints.Num() * sizeof(uint32));
	int32 BreakpointIndex = 0;
	int32 ValidBreakPointsNum = BreakPoints.Num();
	if (!BreakPoints.IsEmpty())
	{
		//对齐数据
		while (BreakpointsInU32[BreakpointIndex] < ContinuousIndexSeries[0] && ValidBreakPointsNum > 0)
		{
			BreakpointIndex++;
			ValidBreakPointsNum--;
		}
		int32 BreakpointIndexLast = BreakPoints.Num() - 1;
		while (BreakpointsInU32[BreakpointIndexLast] > ContinuousIndexSeries.Last(0) && ValidBreakPointsNum > 0)
		{
			BreakpointIndexLast--;
			ValidBreakPointsNum--;
		}
		for (int i = 1; i < BreakpointsInU32.Num(); ++i)
		{
			if (BreakpointsInU32[i - 1] == BreakpointsInU32[i])
			{
				ValidBreakPointsNum--;
			}
		}
	}
	DebugStr = "BreakPoints Array:";
	for (const uint32& Breakpoint : BreakpointsInU32)
	{
		DebugStr += FString::Printf(TEXT("%d,"), Breakpoint);
	}
	UE_LOG(LogTemp, Display, TEXT("%s"), *DebugStr);
	DebugStr.Reset();

	TArray<TArray<uint32>> Results = Subsystem->GetContinuousIndexSeries(
		ContinuousIndexSeries, BreakpointsInU32);
	int ResultsCounter = 0;
	for (int i = 0; i < Results.Num(); ++i)
	{
		DebugStr = FString::Printf(TEXT("Result %d Array:"), i);
		for (int j = 0; j < Results[i].Num(); ++j)
		{
			DebugStr += FString::Printf(TEXT("%d,"), Results[i][j]);
			if (j > 0)
			{
				if (Results[i][j] - Results[i][j - 1] != 1)
				{
					UE_LOG(LogTemp, Error, TEXT("Not Continuous"));
					return false;
				}
			}
			if (BreakpointsInU32.Contains(Results[i][j]))
			{
				UE_LOG(LogTemp, Error, TEXT("Results Contains BreakPoints Elem"));
				return false;
			}
			ResultsCounter++;
		}
		UE_LOG(LogTemp, Display, TEXT("%s"), *DebugStr);
		DebugStr.Reset();
	}
	if (ResultsCounter + ValidBreakPointsNum != EndIndex - StartIndex + 1)
	{
		UE_LOG(LogTemp, Error, TEXT("Lose Elem"));
		return false;
	}
	return true;
}

template <typename T>
bool TestInsertElementsAtIndex(URoadGeneratorSubsystem* Subsystem, TArray<T>& TargetArray,
                               const TMap<int32, TArray<T>>& InsertMap)
{
	int32 ElemNum = 0;
	TArray<int32> IncreasingIndexes;
	TArray<T> BackUpArray = TargetArray;
	for (const auto& IndexElemMap : InsertMap)
	{
		ElemNum += IndexElemMap.Value.Num();
		IncreasingIndexes.Emplace(IndexElemMap.Key);
	}
	IncreasingIndexes.Sort();
	Subsystem->InsertElementsAtIndex(TargetArray, InsertMap);
	//需要实例化模板才能具体判定
	if (TargetArray.Num() != BackUpArray.Num() + ElemNum)
	{
		UE_LOG(LogTemp, Error, TEXT("Lose Elem"))
		return false;
	}
	return true;
}

void FRoadGeneratorSubsystemTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("FRoadGeneratorSubsystemTest_TestName"));
	OutTestCommands.Add(TEXT("FRoadGeneratorSubsystemTest_TestName"));
}

bool FRoadGeneratorSubsystemTest::RunTest(const FString& Parameters)
{
	URoadGeneratorSubsystem* TestingTarget = GEditor->GetEditorSubsystem<URoadGeneratorSubsystem>();
	bool bPassAllTest = true;
	if (!TestingTarget)
	{
		AddError("Get URoadGeneratorSubsystem Failed");
		return false;
	}
	//TestFor GetContinuousIndexSeries()

	{
		//测试用例
		struct FGetContinuousIndexTestCase
		{
			explicit FGetContinuousIndexTestCase(const TArray<int32>& InBreakPoints, int32 InStartIndex,
			                                     int32 InEndIndex):
				Breakpoints(InBreakPoints), StartIndex(InStartIndex), EndIndex(InEndIndex)
			{
			}

			TArray<int32> Breakpoints{};
			int32 StartIndex = 0;
			int32 EndIndex = 10;
		};

		TArray<FGetContinuousIndexTestCase> ContinuousIndexSeriesTestCase;
		//中间切断C0
		ContinuousIndexSeriesTestCase.Emplace(FGetContinuousIndexTestCase{{13, 14, 15}, 0, 21});
		//部分不包含(前后)C1C2
		ContinuousIndexSeriesTestCase.Emplace(FGetContinuousIndexTestCase{{3, 4, 5, 6}, 5, 26});
		//部分不包含(前后)C1C2
		ContinuousIndexSeriesTestCase.Emplace(FGetContinuousIndexTestCase{{18, 19, 20, 21}, 5, 26});
		//分段切断C3
		ContinuousIndexSeriesTestCase.Emplace(FGetContinuousIndexTestCase{{1, 2, 3, 10, 11, 17}, 0, 21});
		//全部切断C4
		ContinuousIndexSeriesTestCase.Emplace(FGetContinuousIndexTestCase{{0, 1, 2}, 0, 2});
		//不切C5
		ContinuousIndexSeriesTestCase.Emplace(FGetContinuousIndexTestCase{{}, 0, 5});
		//函数要求不包含重复值，无重复断点测试用例
		for (int i = 0; i < ContinuousIndexSeriesTestCase.Num(); ++i)
		{
			bool bAC = TestGetContinuousIndexSeries(TestingTarget, ContinuousIndexSeriesTestCase[i].Breakpoints,
			                                        ContinuousIndexSeriesTestCase[i].StartIndex,
			                                        ContinuousIndexSeriesTestCase[i].EndIndex);
			bPassAllTest &= bAC;
			if (!bAC)
			{
				AddError(FString::Printf(TEXT("Fail To Pass Case %d"), i));
			}
		}
	}

	//TestFor InsertElementsAtIndex()
	{
		/*TArray<FString> TargetArray0{"a", "b", "c", "d", "e", "f"};
		TPair<int32, TArray<FString>> InsertElem0{0, TArray<FString>{"0a", "1a", "2a"}};
		TPair<int32, TArray<FString>> InsertElem1{4, TArray<FString>{"0d", "1d"}};
		TPair<int32, TArray<FString>> InsertElem2{5, TArray<FString>{"0f", "1f", "2f"}};
		TMap<int32, TArray<FString>> InsertArray0{InsertElem0, InsertElem1, InsertElem2};
		if (!TestInsertElementsAtIndex(TestingTarget, TargetArray0, InsertArray0))
		{
			AddError("[InsertElementsAtIndex] Case0 Failed");
			bPassAllTest &= false;
		}*/

		TArray<FTransform> TargetArray1{
			FTransform(FRotator(0, 0, 0), FVector(0.0), FVector(0.0)),
			FTransform(FRotator(90, 0, 0), FVector(1.0), FVector(0.0)),
			FTransform(FRotator(0, 90, 0), FVector(2.0), FVector(0.0)),
			FTransform(FRotator(0, 0, 90), FVector(3.0), FVector(0.0))
		};
		TPair<int32, TArray<FTransform>> InsertElem3{
			0,
			TArray<FTransform>{
				FTransform(FRotator(0, 0, 90), FVector(0.0), FVector(1.0)),
				FTransform(FRotator(0, 0, 90), FVector(0.0), FVector(2.0)),
				FTransform(FRotator(0, 0, 90), FVector(0.0), FVector(3.0))
			}
		};
		TPair<int32, TArray<FTransform>> InsertElem4{
			2,
			TArray<FTransform>{
				FTransform(FRotator(90, 0, 0), FVector(2.0), FVector(1.0)),
				FTransform(FRotator(90, 0, 0), FVector(2.0), FVector(2.0)),
				FTransform(FRotator(90, 0, 0), FVector(2.0), FVector(3.0))
			}
		};
		TPair<int32, TArray<FTransform>> InsertElem5{
			3,
			TArray<FTransform>{
				FTransform(FRotator(0, 90, 0), FVector(3.0), FVector(1.0)),
				FTransform(FRotator(0, 90, 0), FVector(3.0), FVector(2.0))
			}
		};
		TMap<int32, TArray<FTransform>> InsertArray1{InsertElem3, InsertElem4, InsertElem5};
		if (!TestInsertElementsAtIndex(TestingTarget, TargetArray1, InsertArray1))
		{
			AddError("[InsertElementsAtIndex] Case1 Failed");
			bPassAllTest &= false;
		}
	}
	return bPassAllTest;
}
#endif
