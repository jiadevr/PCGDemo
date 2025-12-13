#pragma once
#include "CoreMinimal.h"
#include "Kismet/KismetMathLibrary.h"
#include "BuildingPlacementStruct.generated.h"

struct FPlaceableBlockEdge
{
	FVector StartPointWS;
	FVector EndPointWS;
	FVector Direction;
	float Length;
	//这个值可能不连续
	int32 SegmentIndexOfOwnerSpline;
	float UsedLength;
};

USTRUCT(BlueprintType)
struct FPlacedBuilding
{
	GENERATED_BODY()
	FPlacedBuilding()
	{
	};

	FPlacedBuilding(const FVector& InLocation, const FVector& InForward, const FVector& InBuildingExtent,
	                int32 InTypeID = -1, int32 InOwnerBlockEdgeIndex = -1) :
		Location(InLocation), ForwardDir(InForward), BuildingExtent(InBuildingExtent), TypeID(InTypeID),
		OwnerBlockEdgeIndex(InOwnerBlockEdgeIndex)
	{
		//业务逻辑中先初始化一个Dummy-Extent为0避免每次申请内存，因此在这里更新AABB和OBB没有作用
	}

	~FPlacedBuilding()
	{
	};

	FVector Location;
	//垂直于Spline向外
	FVector ForwardDir;
	//存储包围盒尺寸，X方向沿着Spline前进方向，Y方向垂直于Spline样条
	FVector BuildingExtent;
	int32 TypeID;
	int32 OwnerBlockEdgeIndex;

	FBox2D BoundingBox;
	FOrientedBox OBBBox;

	void RefreshCollisionInfo()
	{
		OBBBox.Center = Location;
		OBBBox.AxisY = ForwardDir;
		OBBBox.AxisX = UKismetMathLibrary::Cross_VectorVector(ForwardDir, FVector::UpVector);
		OBBBox.ExtentX = BuildingExtent.X;
		OBBBox.ExtentY = BuildingExtent.Y;
		OBBBox.ExtentZ = BuildingExtent.Z;
		BoundingBox = FBox2D(GetPointsLocation());
	}

	void DrawDebugShape(const UWorld* TargetWorld, const FColor DebugColor = FColor::Red) const
	{
		FVector ArrowEnd = Location + ForwardDir * 100.0;
		DrawDebugDirectionalArrow(TargetWorld, Location, ArrowEnd, 20.0, FColor::Red, true);
		FBox SolidBox(-BuildingExtent, BuildingExtent);
		//X方向是沿样条方向的，但是ForwardDir是垂直样条方向的
		FQuat XDir = UKismetMathLibrary::Cross_VectorVector(ForwardDir, FVector::UpVector).ToOrientationQuat();
		FTransform BoxTransform{XDir, Location};
		DrawDebugSolidBox(TargetWorld, SolidBox, DebugColor, BoxTransform, true, -1, 0);
		DrawDebugBox(TargetWorld->GetWorld(), Location, BuildingExtent,
		             XDir, FColor::Black, true, -1, 0, 30.0f);
	}

	TArray<FVector2D> GetPointsLocation() const
	{
		TArray<FVector2D> Points;
		Points.Init(FVector2D(Location), 4);
		const TArray<FVector> UnitDirs{{1.0, 1.0, 0.0}, {1.0, -1.0, 0.0}, {-1.0, -1.0, 0.0}, {-1.0, 1.0, 0.0}};

		for (int i = 0; i < 4; ++i)
		{
			//这个地方需要根据旋转角度扭转
			FVector XDir = UKismetMathLibrary::Cross_VectorVector(ForwardDir, FVector::UpVector);
			FRotator ActorRotator = UKismetMathLibrary::MakeRotFromX(XDir);
			FTransform LocalTransform(ActorRotator);
			//UE_LOG(LogTemp, Display, TEXT("Rotation: %s"),*ActorRotator.ToString());
			FVector TransformedVector = UKismetMathLibrary::TransformLocation(
				LocalTransform, UnitDirs[i] * BuildingExtent);
			//UE_LOG(LogTemp, Display, TEXT("TransResult: %s"),*TransformedVector.ToString());
			Points[i] += FVector2D(TransformedVector);
			//UE_LOG(LogTemp, Display, TEXT("FinalResult: %s"),*Points[i].ToString());
			/*DrawDebugSphere(GEditor->GetEditorWorldContext().World(), FVector(Points[i], 0.0), 10.0f, 8, FColor::Blue,
			                true);*/
		}
		return Points;
	}

	bool IsOverlappedInOBB(const FPlacedBuilding& OtherBuilding) const
	{
		//使用UE的封装实现
		bool bIsOverlapped = true;
		//对四个轴向分别测试，根据分离轴定律有一条轴上的值不相交即图形不相交
		TArray<FVector> ProjectionAxisArray;
		ProjectionAxisArray.Emplace(OBBBox.AxisX);
		ProjectionAxisArray.Emplace(OBBBox.AxisY);
		ProjectionAxisArray.Emplace(OtherBuilding.OBBBox.AxisX);
		ProjectionAxisArray.Emplace(OtherBuilding.OBBBox.AxisY);
		FFloatInterval ProjectionOfSelf;
		FFloatInterval ProjectionOfOther;
		FFloatInterval IntersectionRange;
		for (const auto& ProjectionAxis : ProjectionAxisArray)
		{
			ProjectionOfSelf = OBBBox.Project(ProjectionAxis);
			ProjectionOfOther = OtherBuilding.OBBBox.Project(ProjectionAxis);
			//Intersect的构造使用两个分段中小的上限作为新上限，用两个中大的下限作为新下限，当两者不相交的时候会造成下限大于上限，对应Invalide区间
			IntersectionRange = Intersect(ProjectionOfSelf, ProjectionOfOther);
			bIsOverlapped &= (IntersectionRange.IsValid());
			if (!bIsOverlapped)
			{
				break;
			}
		}
		return bIsOverlapped;
	}

	bool IsOverlappedByOtherBuilding(const FPlacedBuilding& OtherBuilding) const
	{
		//过滤同一分段上的，已经使用距离控制不会相交
		if (OwnerBlockEdgeIndex != -1 && OtherBuilding.OwnerBlockEdgeIndex == OwnerBlockEdgeIndex)
		{
			return false;
		}
		//首先进行球体范围检测，2D平面那可以简化成圆检测，圆心之间的距离平方
		double DistanceSquared = UKismetMathLibrary::DistanceSquared2D(FVector2D(Location),
		                                                               FVector2D(OtherBuilding.Location));
		if (DistanceSquared > (BuildingExtent.SizeSquared2D() + OtherBuilding.BuildingExtent.SizeSquared2D() + 2 *
			BuildingExtent.Size2D() * OtherBuilding.BuildingExtent.Size2D()))
		{
			//距离大于外接圆半径之和，不会发生相交	
			return false;
		}
		UE_LOG(LogTemp, Display,
		       TEXT("[BuildingGenerator] Potential Intersection Detected!,Fail To Pass Sphere Detection"));

		//然后进行AABB检测，同样做二维平面
		if (!BoundingBox.Intersect(OtherBuilding.BoundingBox))
		{
			return false;
		}
		UE_LOG(LogTemp, Display,
		       TEXT("[BuildingGenerator] Potential Intersection Detected!,Fail To Pass AABB Detection"));
		//最后进行OBB检测
		return IsOverlappedInOBB(OtherBuilding);
	}

	/**
	 * 当前Building和传入的Building进行合并，重新计算Bounding，以调用者朝向为主方向，相当于以现在的体积构造一个新的OBB
	 * @param OtherBuilding 
	 * @return 
	 */
	[[nodiscard]] FPlacedBuilding MergeBuilding(const FPlacedBuilding& OtherBuilding)
	{
		//相当于以现在的
		FVector CurrentAxisX = OBBBox.AxisX;
		FVector CurrentAxisY = OBBBox.AxisY;
		FFloatInterval CurrentIntervalX = OBBBox.Project(CurrentAxisX);
		FFloatInterval OtherIntervalX = OtherBuilding.OBBBox.Project(CurrentAxisX);
		CurrentIntervalX.Include(OtherIntervalX.Max);
		CurrentIntervalX.Include(OtherIntervalX.Min);
		FFloatInterval CurrentIntervalY = OBBBox.Project(CurrentAxisY);
		FFloatInterval OtherIntervalY = OtherBuilding.OBBBox.Project(CurrentAxisY);
		CurrentIntervalY.Include(OtherIntervalY.Max);
		CurrentIntervalY.Include(OtherIntervalY.Min);
		//新建新的临时描述

		FVector NewExtent{
			CurrentIntervalX.Size() / 2, CurrentIntervalY.Size() / 2,
			FMath::Max(BuildingExtent.Z, OtherBuilding.BuildingExtent.Z)
		};
		FVector NewCenter = 0.5 * Location + 0.5 * OtherBuilding.Location;

		FPlacedBuilding DummyBuilding{NewCenter, ForwardDir, NewExtent, -1, OwnerBlockEdgeIndex};
		return DummyBuilding;
	}
};
