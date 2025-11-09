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

/*struct FBuildingInfo
{

}*/;

USTRUCT()
struct FPlacedBuilding
{
	GENERATED_BODY()
	FVector Location;
	FVector ForwardDir;
	FVector BuildingExtent;
	int32 TypeID;
	int32 OwnerBlockEdgeIndex;
};
