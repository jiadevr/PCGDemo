#include "Kismet/KismetStringLibrary.h"
#include "Misc/AutomationTest.h"
#include "Road/RoadGeometryUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(RoadGeometryUtilitiesTest,
                                 "PCGDemo.JIAPCGAidTool.Source.CityGenerator.UnitTestClass.RoadGeometryUtilitiesTest",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool IsParallelTest()
{
	int32 CaseCounter = 0;
	//Case0:2DParallel
	FVector LineAStart = FVector(1, 1, 0);
	FVector LineAEnd = FVector(2, 1, 1);
	FVector LineBStart = FVector(2, 1, 2);
	FVector LineBEnd = FVector(3, 1, 0);
	if (URoadGeometryUtilities::IsParallel(LineAStart, LineAEnd, LineBStart, LineBEnd, true) != true)
	{
		UE_LOG(LogTemp, Error, TEXT("[RoadGeometryUtilitiesTest-IsParallelTest]Test Failed On Case %d"), CaseCounter);
		return false;
	}
	CaseCounter++;
	//Case1:2D NotParallel
	LineAStart = FVector(1, 1, 0);
	LineAEnd = FVector(2, 1, 1);
	LineBStart = FVector(2, 1, 2);
	LineBEnd = FVector(3, 2, 0);
	if (URoadGeometryUtilities::IsParallel(LineAStart, LineAEnd, LineBStart, LineBEnd, true) == true)
	{
		UE_LOG(LogTemp, Error, TEXT("[RoadGeometryUtilitiesTest-IsParallelTest]Test Failed On Case %d"), CaseCounter);
		return false;
	}
	CaseCounter++;
	//Case2：3DParallel
	LineAStart = FVector(1, 1, 0);
	LineAEnd = FVector(2, 1, 1);
	LineBStart = FVector(2, 1, 2);
	LineBEnd = FVector(3, 1, 3);
	if (URoadGeometryUtilities::IsParallel(LineAStart, LineAEnd, LineBStart, LineBEnd, false) != true)
	{
		UE_LOG(LogTemp, Error, TEXT("[RoadGeometryUtilitiesTest-IsParallelTest]Test Failed On Case %d"), CaseCounter);
		return false;
	}
	CaseCounter++;
	//Case2：3DNotParallel
	LineAStart = FVector(1, 1, 0);
	LineAEnd = FVector(2, 1, 1);
	LineBStart = FVector(2, 1, 2);
	LineBEnd = FVector(3, 1, 0);
	if (URoadGeometryUtilities::IsParallel(LineAStart, LineAEnd, LineBStart, LineBEnd, false) == true)
	{
		UE_LOG(LogTemp, Error, TEXT("[RoadGeometryUtilitiesTest-IsParallelTest]Test Failed On Case %d"), CaseCounter);
		return false;
	}
	UE_LOG(LogTemp, Display, TEXT("IsParallelTest PASSED"));
	return true;
}

bool SimplifySplinePointsInlineTest()
{
	auto CheckResult = [](const TArray<FVector>& Original,
	                      const TArray<FVector>& Expected, bool bIgnoreZ = false)-> bool
	{
		TArray<FVector> Copy = Original;
		URoadGeometryUtilities::SimplifySplinePointsInline(Copy, bIgnoreZ);

		if (Copy.Num() != Expected.Num())
		{
			UE_LOG(LogTemp, Error,
			       TEXT(
				       "[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]Failed To Pass,Unmatch Count,Result Num %d,While Case Expecting %d"
			       ), Copy.Num(), Expected.Num())
			return false;
		}

		for (int32 i = 0; i < Expected.Num(); ++i)
		{
			if (!Copy[i].Equals(Expected[i], 0.1))
			{
				UE_LOG(LogTemp, Error,
				       TEXT(
					       "[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]Failed To Pass,Point %d Unmatch,Result Is %s While Case Expecting %s"
				       ),
				       i, *UKismetStringLibrary::Conv_VectorToString(Copy[i]),
				       *UKismetStringLibrary::Conv_VectorToString(Expected[i]));
				return false;
			}
		}
		return true;
	};
	int32 CaseCounter = 0;
	// 0. 空数组
	{
		TArray<FVector> Empty;
		TArray<FVector> ExpectEmpty;
		if (!CheckResult(Empty, ExpectEmpty))
		{
			UE_LOG(LogTemp, Error,
			       TEXT("[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]Test Failed On Case %d"),
			       CaseCounter);
			return false;
		}
	}
	CaseCounter++;
	// 1. 点数 <3（早退）
	{
		TArray<FVector> Two = {FVector(0, 0, 0), FVector(100, 0, 0)};
		TArray<FVector> ExpectTwo = Two;
		if (!CheckResult(Two, ExpectTwo))
		{
			UE_LOG(LogTemp, Error,
			       TEXT("[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]Test Failed On Case %d"),
			       CaseCounter);
			return false;
		}
	}
	CaseCounter++;
	// 2. 全部共线（应只剩首尾）
	{
		TArray<FVector> Line =
		{
			FVector(0, 0, 0),
			FVector(10, 0, 0),
			FVector(20, 0, 0),
			FVector(30, 0, 0),
			FVector(40, 0, 0)
		};
		TArray<FVector> ExpectLine = {Line[0], Line.Last()};
		if (!CheckResult(Line, ExpectLine))
		{
			UE_LOG(LogTemp, Error,
			       TEXT("[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]Test Failed On Case %d"),
			       CaseCounter);
			return false;
		}
	}
	CaseCounter++;
	// 3. 闭合多边形且首尾共线（应删掉重复起点）
	{
		TArray<FVector> Rect =
		{
			FVector(50, 0, 0),
			FVector(100, 0, 0),
			FVector(100, 100, 0),
			FVector(0, 100, 0),
			FVector(0, 0, 0) // 闭合
		};
		// 首尾段共线，应把 index=0 的点删掉
		TArray<FVector> ExpectRect =
		{
			FVector(0, 0, 0),
			FVector(100, 0, 0),
			FVector(100, 100, 0),
			FVector(0, 100, 0),
		};
		if (!CheckResult(Rect, ExpectRect))
		{
			UE_LOG(LogTemp, Error,
			       TEXT("[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]Test Failed On Case %d"),
			       CaseCounter);
			return false;
		}
	}
	CaseCounter++;

	// 4. 开放折线，中间有拐角（应保留拐角）
	{
		TArray<FVector> ZigZag =
		{
			FVector(0, 0, 0),
			FVector(50, 0, 0),
			FVector(50, 50, 0),
			FVector(100, 50, 0)
		};
		TArray<FVector> ExpectZigZag = ZigZag; // 一个角都不少
		if (!CheckResult(ZigZag, ExpectZigZag))
		{
			UE_LOG(LogTemp, Error,
			       TEXT("[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]Test Failed On Case %d"),
			       CaseCounter);
			return false;
		}
	}
	CaseCounter++;

	// 5. 3 点闭合且首尾共线（应只剩 2 点）
	//这个测试可能说明了另一种情况们需要在实际场景中测试看有没有必要覆盖
	/*{
		TArray<FVector> Triangle =
		{
			FVector(25,25, 0),
			FVector(50, 50, 0),
			FVector(0, 0, 0) // 回到起点
		};
		TArray<FVector> ExpectTriangle = {FVector(50, 50, 0), FVector(0, 0, 0)};
		if (!CheckResult(Triangle, ExpectTriangle))
		{
			UE_LOG(LogTemp, Error,
			       TEXT("[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]Test Failed On Case %d"),
			       CaseCounter);
			return false;
		}
	}*/
	UE_LOG(LogTemp, Display, TEXT("[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest] PASSED"));
	return true;
}

bool RoadGeometryUtilitiesTest::RunTest(const FString& Parameters)
{
	bool bSuccess = true;
	//平行函数测试
	bSuccess &= IsParallelTest();
	if (!bSuccess)
	{
		return false;
	}
	//删除点函数测试
	bSuccess &= SimplifySplinePointsInlineTest();
	if (!bSuccess)
	{
		return false;
	}
	return bSuccess;
}
