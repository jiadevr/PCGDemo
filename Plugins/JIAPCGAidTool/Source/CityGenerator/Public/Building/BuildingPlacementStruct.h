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
	int32 SegmentIndexOfOwnerSpline;
	float UsedLength;
};

/*struct FBuildingInfo
{

}*/;

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

protected:
	double GetOverlappedSphereRadiusSquared() const
	{
		double RadiusSquared = BuildingExtent.X * BuildingExtent.X + BuildingExtent.Y * BuildingExtent.Y;
		return RadiusSquared;
	}

	TArray<FVector2D> GetPointsLocation() const
	{
		TArray<FVector2D> Points;
		Points.Init(FVector2D(Location), 4);
		const TArray<FVector> UnitDirs{{1.0, 1.0, 0.0}, {1.0, -1.0, 0.0}, {-1.0, -1.0, 0.0}, {-1.0, 1.0, 0.0}};

		for (int i = 0; i < 4; ++i)
		{
			//这个地方需要根据旋转角度扭转
			FRotator ActorRotator = UKismetMathLibrary::MakeRotFromX(ForwardDir);
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
		TArray<FVector2D> ProjectTargetAxis;
		ProjectTargetAxis.Emplace(FVector2D(ForwardDir));
		ProjectTargetAxis.Emplace(FVector2D(UKismetMathLibrary::Cross_VectorVector(ForwardDir, FVector::UpVector)));
		ProjectTargetAxis.Emplace(FVector2D(OtherBuilding.ForwardDir));
		ProjectTargetAxis.Emplace(FVector2D(
			UKismetMathLibrary::Cross_VectorVector(OtherBuilding.ForwardDir, FVector::UpVector)));

		TArray<FVector2D> AllPointsOfRec0 = GetPointsLocation();
		TArray<FVector2D> AllPointsOfRec1 = OtherBuilding.GetPointsLocation();
		for (int i = 0; i < 4; ++i)
		{
			TArray<double> Rec0ProjectResult;
			TArray<double> Rec1ProjectResult;
			for (int j = 0; j < 4; ++j)
			{
				Rec0ProjectResult.Emplace(UKismetMathLibrary::DotProduct2D(ProjectTargetAxis[i], AllPointsOfRec0[j]));
				Rec1ProjectResult.Emplace(UKismetMathLibrary::DotProduct2D(ProjectTargetAxis[i], AllPointsOfRec1[j]));
			}
			Rec0ProjectResult.Sort();
			Rec1ProjectResult.Sort();
			const double& MaxOfRec0 = Rec0ProjectResult.Last();
			const double& MaxOfRec1 = Rec1ProjectResult.Last();
			const double& MinOfRec0 = Rec0ProjectResult[0];
			const double& MinOfRec1 = Rec1ProjectResult[0];
			if (MaxOfRec0 < MinOfRec1 || MaxOfRec1 < MinOfRec0)
			{
				return false;
			}
		}
		UE_LOG(LogTemp, Display, TEXT("Find Intersection By OBB"))
		return true;
	}

public:
	bool IsOverlappedByOtherBuilding(const FPlacedBuilding& OtherBuilding) const
	{
		//过滤同一条线上的
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
		FBox2D AABBOfOther(OtherBuilding.GetPointsLocation());
		FBox2D AABBOfCurrent(GetPointsLocation());
		//直接用API判断,包围盒不相交则不相交
		if (!AABBOfCurrent.Intersect(AABBOfOther))
		{
			return false;
		}
		UE_LOG(LogTemp, Display,
		       TEXT("[BuildingGenerator] Potential Intersection Detected!,Fail To Pass AABB Detection"));
		//最后进行OBB检测
		return IsOverlappedInOBB(OtherBuilding);
	}
};
