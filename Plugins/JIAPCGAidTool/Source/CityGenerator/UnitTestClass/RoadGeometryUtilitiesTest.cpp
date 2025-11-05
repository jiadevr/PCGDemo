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
		URoadGeometryUtilities::SimplifySplinePointsInline(Copy, bIgnoreZ, 100);
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
	int32 GroupCounter = 0;
	int32 SubCaseCounter = 0;
	// Group 0: 边界条件测试
	{
		GroupCounter = 0;
		SubCaseCounter = 0;
		// 0-0. 空数组
		{
			TArray<FVector> Empty;
			TArray<FVector> ExpectEmpty;
			if (!CheckResult(Empty, ExpectEmpty))
			{
				UE_LOG(LogTemp, Error,
				       TEXT("[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]Test Failed On Group %d-Case %d"
				       ),
				       GroupCounter, SubCaseCounter);
				return false;
			}
		}
		SubCaseCounter++;

		// 0-1. 点数 < 3（早退）
		{
			TArray<FVector> Two = {FVector(0, 0, 0), FVector(100, 0, 0)};
			TArray<FVector> ExpectTwo = Two;
			if (!CheckResult(Two, ExpectTwo))
			{
				UE_LOG(LogTemp, Error,
				       TEXT("[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]Test Failed On Group %d-Case %d"
				       ),
				       GroupCounter, SubCaseCounter);
				return false;
			}
		}
	}

	// Group 1: 距离简化测试（圆弧段简化）
	{
		GroupCounter = 1;
		SubCaseCounter = 0;

		// 1-0. 连续短线段（圆弧）被简化
		{
			TArray<FVector> ArcPoints;
			// 模拟圆弧细分点（短线段）
			for (int32 i = 0; i < 10; ++i)
			{
				double Angle = i * PI / 18.0; // 10度间隔
				ArcPoints.Add(FVector(i * 10.0, 50.0 * FMath::Sin(Angle), 0.0));
			}

			TArray<FVector> Expected;
			Expected.Add(ArcPoints[0]); // 圆弧起点
			Expected.Add(ArcPoints.Last()); // 圆弧终点

			if (!CheckResult(ArcPoints, Expected))
			{
				UE_LOG(LogTemp, Error,
				       TEXT("[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]Test Failed On Group %d-Case %d"
				       ),
				       GroupCounter, SubCaseCounter);
				return false;
			}
		}
		SubCaseCounter++;

		// 1-1. 混合长短线段
		{
			TArray<FVector> MixedPoints = {
				FVector(0, 0, 0),
				FVector(50, 10, 0), // 短线段，有Y方向偏移
				FVector(100, 40, 0), // 短线段，有Y方向偏移
				FVector(500, 40, 0), // 长线段，保持Y方向
				FVector(550, 30, 0), // 短线段，Y方向变化
				FVector(600, 0, 0) // 短线段，Y方向变化
			};

			TArray<FVector> Expected = {
				FVector(0, 0, 0), // 第一段短序列起点
				FVector(100, 40, 0), // 第一段短序列终点
				FVector(500, 40, 0), // 长线段
				FVector(600, 0, 0) // 最后一段短序列终点
			};

			if (!CheckResult(MixedPoints, Expected))
			{
				UE_LOG(LogTemp, Error,
				       TEXT("[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]Test Failed On Group %d-Case %d"
				       ),
				       GroupCounter, SubCaseCounter);
				return false;
			}
		}
	}

	// Group 2: 角度简化测试（共线线段合并）
	{
		GroupCounter = 2;
		SubCaseCounter = 0;

		// 2-0. 近似共线的线段被合并
		{
			TArray<FVector> CollinearPoints = {
				FVector(0, 0, 0),
				FVector(300, 1, 0), // 很小的角度偏移
				FVector(600, 2, 0), // 很小的角度偏移
				FVector(300, 300, 0), // 明显角度变化
				FVector(400, 301, 0) // 很小的角度偏移
			};

			TArray<FVector> Expected = {
				FVector(0, 0, 0),
				FVector(600, 2, 0), // 前三个点共线，保留首尾
				FVector(300, 300, 0),
				FVector(400, 301, 0)
			};

			if (!CheckResult(CollinearPoints, Expected))
			{
				UE_LOG(LogTemp, Error,
				       TEXT("[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]Test Failed On Group %d-Case %d"
				       ),
				       GroupCounter, SubCaseCounter);
				return false;
			}
		}
		SubCaseCounter++;

		// 2-1. 明显角度变化的线段被保留
		{
			TArray<FVector> AnglePoints = {
				FVector(0, 0, 0),
				FVector(100, 0, 0),
				FVector(200, 100, 0), // 45度角变化
				FVector(300, 100, 0),
				FVector(400, 0, 0) // -45度角变化
			};

			TArray<FVector> Expected = AnglePoints;
			Expected.RemoveAtSwap(0, 1); //第一个点应该被去除

			if (!CheckResult(AnglePoints, Expected))
			{
				UE_LOG(LogTemp, Error,
				       TEXT("[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]Test Failed On Group %d-Case %d"
				       ),
				       GroupCounter, SubCaseCounter);
				return false;
			}
		}
	}

	// Group 3: 忽略Z坐标测试
	{
		GroupCounter = 3;
		SubCaseCounter = 0;

		// 3-0. 忽略Z坐标时的距离简化
		{
			TArray<FVector> PointsWithZ = {
				FVector(0, 0, 0),
				FVector(50, 0, 100), // XY平面短线段，Z变化大
				FVector(100, 0, 200), // XY平面短线段，Z变化大
				FVector(500, 0, 0) // XY平面长线段
			};

			TArray<FVector> Expected = {
				FVector(0, 0, 0), // 第一段短序列起点
				FVector(500, 0, 0) // 长线段
			};

			if (!CheckResult(PointsWithZ, Expected, true)) // bIgnoreZ = true
			{
				UE_LOG(LogTemp, Error,
				       TEXT("[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]Test Failed On Group %d-Case %d"
				       ),
				       GroupCounter, SubCaseCounter);
				return false;
			}
		}
		SubCaseCounter++;

		// 3-1. 考虑Z坐标时的距离简化
		{
			TArray<FVector> PointsWithZ = {
				FVector(0, 0, 0),
				FVector(50, 0, 300), // 空间距离长（因为Z变化大）
				FVector(100, 0, 0), // 空间距离长
				FVector(150, 0, 0) // 空间距离短
			};

			TArray<FVector> Expected = {
				FVector(0, 0, 0),
				FVector(50, 0, 300), // 长线段，保留
				FVector(100, 0, 0), // 长线段，保留
				FVector(150, 0, 0) // 最后一点，保留
			};

			if (!CheckResult(PointsWithZ, Expected, false)) // bIgnoreZ = false
			{
				UE_LOG(LogTemp, Error,
				       TEXT("[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]Test Failed On Group %d-Case %d"
				       ),
				       GroupCounter, SubCaseCounter);
				return false;
			}
		}
	}

	// Group 4: 综合测试
	{
		GroupCounter = 4;
		SubCaseCounter = 0;

		// 4-0. 距离简化和角度简化组合测试
		{
			TArray<FVector> ComplexPoints = {
				FVector(0, 0, 0),
				FVector(50, 1, 0), // 短线段 + 小角度
				FVector(100, 2, 0), // 短线段 + 小角度
				FVector(150, 3, 0), // 短线段 + 小角度  
				FVector(500, 400, 0), // 长线段 + 大角度
				FVector(550, 300, 0), // 短线段 + 共线
				FVector(600, 200, 0) // 短线段 + 大角度
			};

			TArray<FVector> Expected = {
				FVector(0, 0, 0), // 第一段短序列起点
				FVector(150, 3, 0), // 第一段短序列终点（距离简化）
				FVector(500, 400, 0), // 长线段起点
				FVector(600, 200, 0) // 最后一点（角度变化大，保留）
			};

			if (!CheckResult(ComplexPoints, Expected))
			{
				UE_LOG(LogTemp, Error,
				       TEXT("[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]Test Failed On Group %d-Case %d"
				       ),
				       GroupCounter, SubCaseCounter);
				return false;
			}
		}
	}

	UE_LOG(LogTemp, Display, TEXT("[RoadGeometryUtilitiesTest-SimplifySplinePointsInlineTest]All Tests Passed!"));
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
