#pragma once
#include "CoreMinimal.h"
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

struct FBuildingInfo
{
	float Length;
	float Width;
	int32 TypeID;
};

USTRUCT()
struct FPlaceBuilding
{
	GENERATED_BODY()
	FVector Location;
	FVector ForwardDir;
	FBuildingInfo* Building;
	int32 OwnerBlockEdgeIndex;
};
