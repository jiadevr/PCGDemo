// Fill out your copyright notice in the Description page of Project Settings.


#include "Building/BuildingGeneratorSubsystem.h"

#include "NotifyUtilities.h"
#include "Building/BuildingDimensionsConfig.h"
#include "Building/BuildingPlacementStruct.h"
#include "CityGenerator/Public/SplineUtilities.h"
#include "Components/SplineComponent.h"
#include "Subsystems/EditorAssetSubsystem.h"

void UBuildingGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UBuildingGeneratorSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UBuildingGeneratorSubsystem::PlaceBuildingAlongSpline(const USplineComponent* TargetSpline)
{
	//0.基础检查
	if (nullptr == TargetSpline)
	{
		return;
	}
	//1.随机生成建筑三维(的一半，相当于Extent)，对建筑维度进行排序，建立大顶堆
	TArray<FVector> BuildingsExtents = GetRandomBuildingConfig();
	//长度优先，深度第二，高度第三
	BuildingsExtents.Sort([](const FVector& A, const FVector& B)
	{
		if (A.X != B.X)
		{
			return A.X > B.X;
		}
		else
		{
			if (A.Y != B.Y)
			{
				return A.Z > B.Z;
			}
			return A.Y > B.Y;
		}
	});
	const float MinimalBuildingLength = BuildingsExtents.Last().X * 2.0;
	//2.构造离散PlaceableEdge
	TArray<FPlaceableBlockEdge> PlaceableEdges;
	const int32 SplineSegmentNum = TargetSpline->GetNumberOfSplineSegments();
	for (int i = 0; i < SplineSegmentNum; ++i)
	{
		float SegmentLength = USplineUtilities::GetSplineSegmentLength(TargetSpline, i);
		if (MinimalBuildingLength > SegmentLength)
		{
			UE_LOG(LogTemp, Display,
			       TEXT("Spline Segment%d Length(%f) Is Too Small To Place Building,Witch Length Is %f"), i,
			       SegmentLength, MinimalBuildingLength);
			continue;
		}
		FVector StartLocation = TargetSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
		FVector EndLocation = TargetSpline->GetLocationAtSplinePoint((i + 1) % SplineSegmentNum,
		                                                             ESplineCoordinateSpace::World);
		FVector SegmentDir = (EndLocation - StartLocation).GetSafeNormal();
		PlaceableEdges.Emplace(StartLocation, EndLocation, SegmentDir, SegmentLength, i, 0);
	}
	for (const FPlaceableBlockEdge& PlaceableEdge : PlaceableEdges)
	{
		UE_LOG(LogTemp, Display, TEXT("SegmentID %d,From%s,To%s Length:%f"), PlaceableEdge.SegmentIndexOfOwnerSpline,
		       *PlaceableEdge.StartPointWS.ToString(), *PlaceableEdge.EndPointWS.ToString(), PlaceableEdge.Length);
	}
	//3.建立局部网格处理碰撞
	//Sphere空间距离检测、AABB检测、OBB检测
	
	//4.处理每条边放置
	//主要思路：每条边优先放置尺寸较大的对象，且优先用完数组中的元素
	//贪心，01背包问题
	//如果想把大的放在中间在边结构里嵌入一个树，分左右，每次尽量往中间放，然后用选择回退-备忘录优化

	for (FPlaceableBlockEdge& PlaceableEdge : PlaceableEdges)
	{
		auto GetBuildingLocation = [&PlaceableEdge](const FVector& FacingDir, float DistanceOfCenter,
		                                            float InsetOffset)-> FVector
		{
			return PlaceableEdge.StartPointWS + PlaceableEdge.Direction * DistanceOfCenter + FacingDir * (-InsetOffset);
		};
		float UsedDistance = 0.0f;
		TSet<int32> UsedID;
		UsedID.Reserve(BuildingsExtents.Num());
		TArray<FPlacedBuilding> SelectedBuildings;
		FVector FacingDir = -PlaceableEdge.Direction.Cross(FVector::UpVector);
		FVector EdgeMidPoint = 0.5 * PlaceableEdge.StartPointWS + 0.5 * PlaceableEdge.EndPointWS;
		FColor DebugColor{FColor::MakeRandomColor()};
		DrawDebugDirectionalArrow(TargetSpline->GetWorld(), EdgeMidPoint, EdgeMidPoint + FacingDir * 200.0f, 20.0f,
		                          DebugColor, true, -1, 0, 5.0f);
		while (UsedDistance < PlaceableEdge.Length - MinimalBuildingLength)
		{
			int32 SelectedIndex = INDEX_NONE;
			FVector BuildingCenter{0.0};
			float MinRemainLength = PlaceableEdge.Length;
			//遍历尝试，寻找一个最合适的
			for (int32 i = 0; i < BuildingsExtents.Num(); ++i)
			{
				if (UsedID.Contains(i)) { continue; }
				if (BuildingsExtents[i].X * 2.0 > MinRemainLength)
				{
					UsedID.Emplace(i);
					continue;
				}
				BuildingCenter = GetBuildingLocation(FacingDir, (UsedDistance +
					                                     BuildingsExtents[i].X), BuildingsExtents[i].Y);
				/*PlaceableEdge.StartPointWS + PlaceableEdge.Direction * (UsedDistance +
				BuildingsExtents[i].X) + FacingDir * (-BuildingsExtents[i].Y);*/
				bool bCanPlace = false;
				//这里需要补充重叠检测信息
				if (true/*bCanPlace*/)
				{
					float RemainLength = PlaceableEdge.Length - (UsedDistance + BuildingsExtents[i].X * 2.0);
					//剩余空间不足以放置最小元素
					if (RemainLength < MinimalBuildingLength)
					{
						SelectedIndex = i;
						break;
					}
					if (RemainLength < MinRemainLength)
					{
						//选择这个元素
						MinRemainLength = RemainLength;
						SelectedIndex = i;
					}
					//非最优解
				}
			}
			if (SelectedIndex != INDEX_NONE)
			{
				FVector SelectedLocation = GetBuildingLocation(FacingDir, (UsedDistance +
					                                               BuildingsExtents[SelectedIndex].X),
				                                               BuildingsExtents[SelectedIndex].Y)
					+ FVector::UpVector * BuildingsExtents[SelectedIndex].Z;
				UsedDistance += BuildingsExtents[SelectedIndex].X * 2.0;
				FPlacedBuilding NewSelected{
					SelectedLocation, FacingDir, BuildingsExtents[SelectedIndex], SelectedIndex,
					PlaceableEdge.SegmentIndexOfOwnerSpline
				};
				UsedID.Add(SelectedIndex);
				SelectedBuildings.Add(NewSelected);
			}
			else
			{
				//全部都尝试过但依然没有合适的，但同时剩余距离还能放，说明会发生重复
				UE_LOG(LogTemp, Warning,
				       TEXT("No Alternative BuildingLeft,There Will Spawn Building With Duplicated Size!"))
				UsedID.Reset();
			}
		}
		ensureAlways(!SelectedBuildings.IsEmpty());
		for (const FPlacedBuilding& SelectedBuilding : SelectedBuildings)
		{
			FRotator FacingRotator{UKismetMathLibrary::MakeRotFromX(PlaceableEdge.Direction)};
			DrawDebugSphere(TargetSpline->GetWorld(), SelectedBuilding.Location, 20.0f, 8, DebugColor, true, -1, 0,
			                5.0f);
			DrawDebugBox(TargetSpline->GetWorld(), SelectedBuilding.Location, SelectedBuilding.BuildingExtent,
			             FacingRotator.Quaternion(), FColor::Black, true, -1, 0, 30.0f);
			FBox SolidBox(-SelectedBuilding.BuildingExtent, SelectedBuilding.BuildingExtent);
			FTransform BoxTransform{FacingRotator.Quaternion(), SelectedBuilding.Location};
			DrawDebugSolidBox(TargetSpline->GetWorld(), SolidBox, DebugColor, BoxTransform, true, -1, 0);
			UE_LOG(LogTemp, Display, TEXT("Select %dth Element,Extent:%s,TargetSplineLength:%f"),
			       SelectedBuilding.TypeID, *SelectedBuilding.BuildingExtent.ToString(), PlaceableEdge.Length)
		}
	}
	return;
}

TArray<FVector> UBuildingGeneratorSubsystem::GetRandomBuildingConfig(int32 Count)
{
	TArray<FVector> BuildingBoxes;
	if (nullptr == BuildingConfig)
	{
		InitialConfigDataAsset();
		if (nullptr == BuildingConfig)
		{
			return BuildingBoxes;
		}
	}
	for (int32 i = 0; i < Count; i++)
	{
		BuildingBoxes.Emplace(BuildingConfig.Get()->GetRandomHalfDimension());
	}
	return BuildingBoxes;
}

void UBuildingGeneratorSubsystem::InitialConfigDataAsset()
{
	UEditorAssetSubsystem* AssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	if (AssetSubsystem && AssetSubsystem->DoesAssetExist(BuildingConfigPath))
	{
		UObject* BuildingConfigDA = AssetSubsystem->LoadAsset(BuildingConfigPath);
		BuildingConfig = Cast<UBuildingDimensionsConfig>(BuildingConfigDA);
	}
	ensureMsgf(nullptr!=BuildingConfig, TEXT("Load Building Config Failed"));
}
