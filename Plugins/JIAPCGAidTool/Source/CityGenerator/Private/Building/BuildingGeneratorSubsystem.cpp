// Fill out your copyright notice in the Description page of Project Settings.


#include "Building/BuildingGeneratorSubsystem.h"

#include "NotifyUtilities.h"
#include "Building/BuildingDimensionsConfig.h"
#include "Building/BuildingPlacementStruct.h"
#include "CityGenerator/Public/SplineUtilities.h"
#include "Components/SplineComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/KismetStringLibrary.h"
#include "Subsystems/EditorAssetSubsystem.h"

DEFINE_LOG_CATEGORY(LogCityGenerator)

void UBuildingGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UBuildingGeneratorSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UBuildingGeneratorSubsystem::PlaceBuildingAlongSpline(USplineComponent* TargetSpline)
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
	for (int i = 0; i < BuildingsExtents.Num(); i++)
	{
		UE_LOG(LogCityGenerator, Display, TEXT("ExtentIndex %d,Value:%s"), i, *BuildingsExtents[i].ToString());
	}
	const float MinimalBuildingLength = BuildingsExtents.Last().X * 2.0;
	//2.构造离散PlaceableEdge
	TArray<FPlaceableBlockEdge> PlaceableEdges;
	const int32 SplineSegmentNum = TargetSpline->GetNumberOfSplineSegments();
	for (int i = 0; i < SplineSegmentNum; ++i)
	{
		float SegmentLength = USplineUtilities::GetSplineSegmentLength(TargetSpline, i);
		if (MinimalBuildingLength > SegmentLength)
		{
			UE_LOG(LogCityGenerator, Display,
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
		UE_LOG(LogCityGenerator, Display, TEXT("SegmentID %d,From%s,To%s Length:%f"),
		       PlaceableEdge.SegmentIndexOfOwnerSpline,
		       *PlaceableEdge.StartPointWS.ToString(), *PlaceableEdge.EndPointWS.ToString(), PlaceableEdge.Length);
	}
	//测试

	/*LastOperatorSpline = TWeakObjectPtr<USplineComponent>(TargetSpline);

	if (!PlaceableEdges.IsEmpty())
	{
		CurrentEdgeIndex = 0;
		PlaceableEdges_Test = PlaceableEdges;
		BuildingsExtentArray = BuildingsExtents;
	}*/

	//3.处理每条边放置
	//主要思路：每条边优先放置尺寸较大的对象，且优先用完数组中的元素
	//贪心，01背包问题
	//如果想把大的放在中间在边结构里嵌入一个树，分左右，每次尽量往中间放，然后用选择回退-备忘录优化
	TArray<FPlacedBuilding> SelectedBuildings;
	for (int i = 0; i < PlaceableEdges.Num(); ++i)
	{
		PlaceBuildingsAtEdge(PlaceableEdges, i, BuildingsExtents, SelectedBuildings);
	}


	/*
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
			FPlacedBuilding NewSelected{
				FVector::ZeroVector, FacingDir, FVector::ZeroVector, -1,
				PlaceableEdge.SegmentIndexOfOwnerSpline
			};
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
					//PlaceableEdge.StartPointWS + PlaceableEdge.Direction * (UsedDistance +
					//BuildingsExtents[i].X) + FacingDir * (-BuildingsExtents[i].Y);
					NewSelected.Location = BuildingCenter;
					NewSelected.BuildingExtent = BuildingsExtents[i];
					bool bCanPlace = true;
					for (const FPlacedBuilding& PlacedBuilding : SelectedBuildings)
					{
						bCanPlace &= (!NewSelected.IsOverlappedByOtherBuilding(PlacedBuilding));
					}
					if (bCanPlace)
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
					//NewSelected.Location = SelectedLocation;
					//NewSelected.BuildingExtent = BuildingsExtents[SelectedIndex];
					NewSelected.TypeID = SelectedIndex;
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
			//Debug绘制
			for (const FPlacedBuilding& SelectedBuilding : SelectedBuildings)
			{
				FRotator FacingRotator{UKismetMathLibrary::MakeRotFromX(PlaceableEdge.Direction)};
				DrawDebugSphere(TargetSpline->GetWorld(), SelectedBuilding.Location, 20.0f, 8, DebugColor, true, -1, 0,
				                5.0f);
				DrawDebugBox(TargetSpline->GetWorld(), SelectedBuilding.Location, SelectedBuilding.BuildingExtent,
				             FacingRotator.Quaternion(), FColor::Black, true, -1, 0, 30.0f);
				//FBox SolidBox(-SelectedBuilding.BuildingExtent, SelectedBuilding.BuildingExtent);
				//FTransform BoxTransform{FacingRotator.Quaternion(), SelectedBuilding.Location};
				//DrawDebugSolidBox(TargetSpline->GetWorld(), SolidBox, DebugColor, BoxTransform, true, -1, 0);
				SelectedBuilding.DrawDebugShape(TargetSpline->GetWorld(), DebugColor);
				UE_LOG(LogTemp, Display, TEXT("Select %dth Element,Extent:%s,TargetSplineLength:%f"),
				       SelectedBuilding.TypeID, *SelectedBuilding.BuildingExtent.ToString(), PlaceableEdge.Length)
			}
		}*/
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

void UBuildingGeneratorSubsystem::TestOverlappingUsingSelectedMeshBox(const AStaticMeshActor* InA,
                                                                      const AStaticMeshActor* InB)
{
	if (nullptr == InA || nullptr == InB)
	{
		UE_LOG(LogCityGenerator, Display, TEXT("Invalid Box Input"));
		return;
	}
	FVector ActorLocation = InA->GetActorLocation();
	FVector BoundsMax;
	FVector BoundsMin;
	InA->GetStaticMeshComponent()->GetLocalBounds(BoundsMin, BoundsMax);
	FVector BoundingBox = BoundsMax * InA->GetStaticMeshComponent()->GetComponentScale();
	FPlacedBuilding BoxA(InA->GetActorLocation(), InA->GetActorForwardVector(), BoundingBox);

	InB->GetStaticMeshComponent()->GetLocalBounds(BoundsMin, BoundsMax);
	BoundingBox = BoundsMax * InB->GetStaticMeshComponent()->GetComponentScale();
	FPlacedBuilding BoxB(InB->GetActorLocation(), InB->GetActorForwardVector(), BoundingBox);
	bool bIsOverlapped = BoxA.IsOverlappedByOtherBuilding(BoxB);
	if (bIsOverlapped)
	{
		InA->GetStaticMeshComponent()->SetCustomPrimitiveDataFloat(0, 1);
		InB->GetStaticMeshComponent()->SetCustomPrimitiveDataFloat(0, 1);
		UE_LOG(LogCityGenerator, Warning, TEXT("Overlapped Building"));
	}
	else
	{
		InA->GetStaticMeshComponent()->SetCustomPrimitiveDataFloat(0, 0);
		InB->GetStaticMeshComponent()->SetCustomPrimitiveDataFloat(0, 0);
	}
}

/*void UBuildingGeneratorSubsystem::TestAddABuilding()
{
	if (!LastOperatorSpline.IsValid() || !PlaceableEdges_Test.IsValidIndex(CurrentEdgeIndex)) { return; }
	FPlaceableBlockEdge& TargetBlockEdge = PlaceableEdges_Test[CurrentEdgeIndex];
	auto GetBuildingLocation = [&TargetBlockEdge](const FVector& FacingDir, float DistanceOfCenter,
	                                              float InsetOffset)-> FVector
	{
		return TargetBlockEdge.StartPointWS + TargetBlockEdge.Direction * DistanceOfCenter + FacingDir * (-InsetOffset);
	};

	FVector FacingDir = -TargetBlockEdge.Direction.Cross(FVector::UpVector);
	FPlacedBuilding NewSelected{
		FVector::ZeroVector, FacingDir, FVector::ZeroVector, -1,
		TargetBlockEdge.SegmentIndexOfOwnerSpline
	};

	const float MinimalBuildingLength = BuildingsExtentArray.Last().X * 2.0;
	//对于最后一段，还有0造成的死区
	const float EndDeadLength = CurrentEdgeIndex == (PlaceableEdges_Test.Num() - 1)
		                            ? GetDeadLength(PlaceableEdges_Test, CurrentEdgeIndex + 1, PlacedBuildingsGlobal)
		                            : 0.0f;

	if (DistanceUsedInSingleLine < TargetBlockEdge.Length - MinimalBuildingLength - EndDeadLength)
	{
		int32 SelectedIndex = INDEX_NONE;
		FVector BuildingCenter{0.0};
		float MinRemainLength = TargetBlockEdge.Length;
		//遍历尝试，寻找一个最合适的
		for (int32 i = 0; i < BuildingsExtentArray.Num(); ++i)
		{
			const FVector& TestingExtent = BuildingsExtentArray[i];
			if (UsedIDInSingleLine.Contains(i)) { continue; }
			if (BuildingsExtentArray[i].X * 2.0 > MinRemainLength)
			{
				UsedIDInSingleLine.Emplace(i);
				continue;
			}
			BuildingCenter = GetBuildingLocation(FacingDir, (DistanceUsedInSingleLine +
				                                     TestingExtent.X), TestingExtent.Y);
			DrawDebugSphere(GEditor->GetEditorWorldContext().World(), BuildingCenter, 50.0f, 8,
			                DebugColorOfSingleLine,
			                true);
			DrawDebugString(GEditor->GetEditorWorldContext().World(), BuildingCenter,
			                FString::Printf(TEXT("BuildingIndex %d"), SelectedIndex), 0, DebugColorOfSingleLine,
			                -1);
			NewSelected.Location = BuildingCenter;
			NewSelected.BuildingExtent = TestingExtent;
			bool bCanPlace = true;
			for (const FPlacedBuilding& PlacedBuilding : PlacedBuildingsGlobal)
			{
				bCanPlace &= !NewSelected.IsOverlappedByOtherBuilding(PlacedBuilding);
			}
			if (bCanPlace)
			{
				float RemainLength = TargetBlockEdge.Length - (DistanceUsedInSingleLine + TestingExtent.X * 2.0);
				//剩余空间不足以放置最小元素
				if (RemainLength < MinimalBuildingLength)
				{
					SelectedIndex = i;
					UE_LOG(LogCityGenerator, Display,
					       TEXT("Select Building,Reason： RemainingLength Smaller Than Threshold"));
					break;
				}
				if (RemainLength < MinRemainLength)
				{
					MinRemainLength = RemainLength;
					SelectedIndex = i;
				}
			}
			else
			{
				UE_LOG(LogCityGenerator, Warning,
				       TEXT("Abort Adding Building,Reason: Failed To Pass Collision Test"))
			}
		}
		if (SelectedIndex != INDEX_NONE)
		{
			NewSelected.BuildingExtent = BuildingsExtentArray[SelectedIndex];
			NewSelected.Location = GetBuildingLocation(FacingDir, (DistanceUsedInSingleLine +
				                                           NewSelected.BuildingExtent.X),
			                                           NewSelected.BuildingExtent.Y) + FVector::UpVector *
				NewSelected.BuildingExtent.Z;

			MarkLocationOnEdge(PlaceableEdges_Test[NewSelected.OwnerBlockEdgeIndex], DistanceUsedInSingleLine,
			                   DebugColorOfSingleLine);
			DistanceUsedInSingleLine += BuildingsExtentArray[SelectedIndex].X * 2.0;
			NewSelected.TypeID = SelectedIndex;
			UsedIDInSingleLine.Add(SelectedIndex);
			PlacedBuildingsGlobal.Add(NewSelected);
			NewSelected.DrawDebugShape(GEditor->GetEditorWorldContext().World(), DebugColorOfSingleLine);
			UE_LOG(LogCityGenerator, Display, TEXT("Place A New Building[Extent: %s ,At Location %s] :"),
			       *NewSelected.BuildingExtent.ToString(), *NewSelected.Location.ToString())
		}
		else
		{
			//全部都尝试过但依然没有合适的，但同时剩余距离还能放，说明会发生重复
			UE_LOG(LogCityGenerator, Warning,
			       TEXT("No Alternative BuildingLeft,There Will Spawn Building With Duplicated Size!"))
			UsedIDInSingleLine.Reset();
		}
	}
	//剩余位置已经放不下了
	else
	{
		UE_LOG(LogCityGenerator, Warning, TEXT("Switch To Next Edge"))
		//粗略计算死区，避免以为和旁边建筑交叉导致无法放置
		CurrentEdgeIndex++;
		if (CurrentEdgeIndex >= PlaceableEdges_Test.Num())
		{
			UE_LOG(LogCityGenerator, Display, TEXT("Finish Building Generator"))
			return;
		}
		DistanceUsedInSingleLine = GetDeadLength(PlaceableEdges_Test, CurrentEdgeIndex, PlacedBuildingsGlobal);
		DebugColorOfSingleLine = FColor::MakeRandomColor();
		MarkLocationOnEdge(PlaceableEdges_Test[CurrentEdgeIndex], DistanceUsedInSingleLine, DebugColorOfSingleLine);
		UsedIDInSingleLine.Reset();
	}
}*/

void UBuildingGeneratorSubsystem::PlaceBuildingsAtEdge(const TArray<FPlaceableBlockEdge>& InAllEdges,
                                                       int32 InTargetEdgeIndex,
                                                       const TArray<FVector>& InAllBuildingExtent,
                                                       TArray<FPlacedBuilding>& PlacedBuildings)
{
	ensureMsgf(InAllEdges.IsValidIndex(InTargetEdgeIndex), TEXT("[CityGenerator]InValid TargetIndex"));
	int32 BuildingCountBeforeAdding = PlacedBuildings.Num();
	//信息准备
	const FColor EdgeDebugColor = FColor::MakeRandomColor();
	const FPlaceableBlockEdge& TargetBlockEdge = InAllEdges[InTargetEdgeIndex];
	auto GetBuildingLocation = [&TargetBlockEdge](const FVector& FacingDir, float DistanceOfCenter,
	                                              float InsetOffset)-> FVector
	{
		return TargetBlockEdge.StartPointWS + TargetBlockEdge.Direction * DistanceOfCenter + FacingDir * (-InsetOffset);
	};
	const FVector FacingDir = -TargetBlockEdge.Direction.Cross(FVector::UpVector);
	FPlacedBuilding NewSelected{
		FVector::ZeroVector, FacingDir, FVector::ZeroVector, -1,
		TargetBlockEdge.SegmentIndexOfOwnerSpline
	};
	//标记使用过的ID，尽量不重复
	TSet<int32> UsedIDs;
	const float MinimalBuildingLength = InAllBuildingExtent.Last().X * 2.0;


	//计算头部死区
	float UsedLength = GetDeadLength(InAllEdges,InTargetEdgeIndex, PlacedBuildings);
	//对于最后一段，还有0造成的死区
	const float EndDeadLength = InTargetEdgeIndex == (InAllEdges.Num() - 1)
		                            ? GetDeadLength(InAllEdges, InTargetEdgeIndex + 1, PlacedBuildings)
		                            : 0.0f;
	while (UsedLength < TargetBlockEdge.Length - MinimalBuildingLength - EndDeadLength)
	{
		int32 SelectedIndex = INDEX_NONE;
		FVector BuildingCenter{0.0};
		float MinRemainLength = TargetBlockEdge.Length;
		//遍历尝试，寻找一个最合适的
		//@TODO：可以用回溯算法配合二分查找获得空余最少的。
		for (int32 i = 0; i < InAllBuildingExtent.Num(); ++i)
		{
			const FVector& TestingExtent = InAllBuildingExtent[i];
			if (UsedIDs.Contains(i)) { continue; }
			//尺寸不合适的
			if (InAllBuildingExtent[i].X * 2.0 > MinRemainLength)
			{
				UsedIDs.Add(i);
				continue;
			}
			BuildingCenter = GetBuildingLocation(FacingDir, (UsedLength +
				                                     TestingExtent.X), TestingExtent.Y);
			NewSelected.Location = BuildingCenter;
			NewSelected.BuildingExtent = TestingExtent;
			bool bCanPlace = true;
			for (const FPlacedBuilding& PlacedBuilding : PlacedBuildings)
			{
				bCanPlace &= !NewSelected.IsOverlappedByOtherBuilding(PlacedBuilding);
			}
			if (bCanPlace)
			{
				const float RemainLength = TargetBlockEdge.Length - (UsedLength + TestingExtent.X * 2.0);
				//剩余空间不足以放置最小元素
				if (RemainLength < MinimalBuildingLength)
				{
					SelectedIndex = i;
					UE_LOG(LogCityGenerator, Display,
					       TEXT("Select Building,Reason： RemainingLength Smaller Than Threshold"));
					break;
				}
				if (RemainLength < MinRemainLength)
				{
					MinRemainLength = RemainLength;
					SelectedIndex = i;
				}
			}
			else
			{
				UE_LOG(LogCityGenerator, Warning,
				       TEXT("Abort Adding Building Size: %s,Reason: Failed To Pass Collision Test"),
				       *TestingExtent.ToString())
			}
		}
		if (SelectedIndex != INDEX_NONE)
		{
			NewSelected.BuildingExtent = InAllBuildingExtent[SelectedIndex];
			NewSelected.Location = GetBuildingLocation(FacingDir, (UsedLength +
				                                           NewSelected.BuildingExtent.X),
			                                           NewSelected.BuildingExtent.Y) + FVector::UpVector *
				NewSelected.BuildingExtent.Z;

			MarkLocationOnEdge(InAllEdges[NewSelected.OwnerBlockEdgeIndex], UsedLength,
			                   EdgeDebugColor);
			UsedLength += InAllBuildingExtent[SelectedIndex].X * 2.0;
			NewSelected.TypeID = SelectedIndex;
			UsedIDs.Add(SelectedIndex);
			PlacedBuildings.Add(NewSelected);
			NewSelected.DrawDebugShape(GEditor->GetEditorWorldContext().World(), EdgeDebugColor);
			UE_LOG(LogCityGenerator, Display, TEXT("Place A New Building[Extent: %s ,At Location %s] :"),
			       *NewSelected.BuildingExtent.ToString(), *NewSelected.Location.ToString())
		}
		else
		{
			//全部都尝试过但依然没有合适的，但同时剩余距离还能放，说明会发生重复
			UE_LOG(LogCityGenerator, Warning,
			       TEXT("No Alternative BuildingLeft,There Will Spawn Building With Duplicated Size!"))
			UsedIDs.Reset();
		}
	}
	//剩余位置已经放不下了
	UE_LOG(LogTemp, Display, TEXT("Finish Placing In Edge Which Index Is %d,Add %d Building(s)"), InTargetEdgeIndex,
	       PlacedBuildings.Num()-BuildingCountBeforeAdding);
	return;
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

bool UBuildingGeneratorSubsystem::CanPlaceNewBuilding(const FPlacedBuilding& NewBuilding,
                                                      const TArray<FPlacedBuilding>& Placed)
{
	for (const auto& Existed : Placed)
	{
		if (NewBuilding.IsOverlappedByOtherBuilding(Existed))
		{
			return false;
		}
	}
	return true;
}

void UBuildingGeneratorSubsystem::MarkLocationOnEdge(const FPlaceableBlockEdge& TargetEdge, float Distance,
                                                     const FColor& DebugColor)
{
	const FVector LineStart = TargetEdge.StartPointWS;
	const FVector LineEnd = LineStart + TargetEdge.Direction * Distance;
	DrawDebugDirectionalArrow(GEditor->GetEditorWorldContext().World(), LineStart, LineEnd, 200.0f, DebugColor, true);
}

/*void UBuildingGeneratorSubsystem::ClearPlacedBuildings()
{
	FlushPersistentDebugLines(GEditor->GetEditorWorldContext().World());
	CurrentEdgeIndex = 0;
	DebugColorOfSingleLine = FColor::Red;
	DistanceUsedInSingleLine = 0.0f;
	UsedIDInSingleLine.Reset();
	PlacedBuildingsGlobal.Reset();
}*/

float UBuildingGeneratorSubsystem::GetDeadLength(const TArray<FPlaceableBlockEdge>& InAllPlaceableEdges,
                                                 int32 InCurrentEdgeIndex,
                                                 const TArray<FPlacedBuilding>& InPlacedBuildings) const
{
	if (InCurrentEdgeIndex == 0 || InPlacedBuildings.IsEmpty())
	{
		return 0.0f;
	}
	ensureMsgf(InAllPlaceableEdges.IsValidIndex(InCurrentEdgeIndex-1)||(InCurrentEdgeIndex==InAllPlaceableEdges.Num()),
	           TEXT("Passed In Index Is Out Of EdgeArray Range"));
	//对于常规边死区由上一组最后一个元素造成，对于最后一条边的尾端死区由首元素造成
	const FPlacedBuilding& DeadLengthMaker = (InCurrentEdgeIndex != InAllPlaceableEdges.Num())
		                                         ? InPlacedBuildings.Last()
		                                         : InPlacedBuildings[0];
	//投影线一般是当前边，对于最后一条边的尾端死区还是最后一条边
	const FVector ProjectionTargetEdgeDir = InAllPlaceableEdges[FMath::Clamp(
		InCurrentEdgeIndex, 0, InAllPlaceableEdges.Num() - 1)].Direction;
	const FVector ProjectionSourceEdgeDir = ((InCurrentEdgeIndex != InAllPlaceableEdges.Num())
		                                         ? InAllPlaceableEdges[InCurrentEdgeIndex - 1]
		                                         : InAllPlaceableEdges[0]).Direction;
	//死区分为两种情况：
	const FVector RecYVector = -DeadLengthMaker.ForwardDir * (2.0 * (DeadLengthMaker.BuildingExtent.Y));
	double AngleCosValue = UKismetMathLibrary::Dot_VectorVector(ProjectionTargetEdgeDir, ProjectionSourceEdgeDir);
	float DeadLength = 0.0f;
	//1.当两边方向向量夹角为锐角时（角点视觉呈现为钝角）仅有一段死区，为矩形Y方向长度向当前边投影；
	if (AngleCosValue > 0.0f)
	{
		DeadLength += FMath::Abs(UKismetMathLibrary::Dot_VectorVector(ProjectionTargetEdgeDir, RecYVector));
	}
	//2当两边方向向量夹角为顿角时（角点视觉呈现为锐角）有两段死区，为矩形Y方向长度向当前边投影+顶面X方向向当前边投影
	else
	{
		//第一段以Y方向边为对侧直角边，sin(a)=sin(pi-a)=YLength/DeadLength1
		double AngleSinValue = FMath::Abs(
			UKismetMathLibrary::CrossProduct2D(FVector2D(ProjectionTargetEdgeDir), FVector2D(ProjectionSourceEdgeDir)));
		DeadLength += RecYVector.Length() / AngleSinValue;
		//第二段直接用X方向边长表示向目标边投影
		const FVector& RecXVector = ProjectionSourceEdgeDir * DeadLengthMaker.BuildingExtent.X * 2;
		DeadLength += FMath::Abs(UKismetMathLibrary::Dot_VectorVector(ProjectionTargetEdgeDir, RecXVector));
	}
	DeadLength += 50.0;
	return DeadLength;
}
