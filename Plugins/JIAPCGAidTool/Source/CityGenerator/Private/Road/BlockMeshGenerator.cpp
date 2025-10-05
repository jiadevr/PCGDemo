// Fill out your copyright notice in the Description page of Project Settings.


#include "Road/BlockMeshGenerator.h"

#include "NotifyUtilities.h"
#include "Components/DynamicMeshComponent.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "Kismet/KismetMathLibrary.h"

int32 UBlockMeshGenerator::BlockGlobalIndex = -1;
// Sets default values for this component's properties
UBlockMeshGenerator::UBlockMeshGenerator()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	GlobalIndex = BlockGlobalIndex;
	BlockGlobalIndex++;
	// ...
}

void UBlockMeshGenerator::SetSweepPath(const TArray<FVector>& InSweepPath)
{
	AActor* CompOwner = GetOwner();
	ensureAlways(nullptr!=CompOwner);
	const FTransform OwnerTransform = CompOwner->GetTransform();
	for (const FVector& PathPoint : InSweepPath)
	{
		SweepPath.Emplace(UKismetMathLibrary::InverseTransformLocation(OwnerTransform, PathPoint));
	}
}

void UBlockMeshGenerator::SetMeshComponent(class UDynamicMeshComponent* InMeshComponent)
{
	if (InMeshComponent != nullptr)
	{
		MeshComponent = TWeakObjectPtr<UDynamicMeshComponent>(InMeshComponent);
	}
}

bool UBlockMeshGenerator::GenerateMesh()
{
	AActor* Owner = GetOwner();
	ensureAlwaysMsgf(nullptr!=Owner, TEXT("Component Has No Owner"));
	if (!MeshComponent.IsValid())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner(
			FString::Printf(
				TEXT("[ERROR]%s Create Intersection Failed,Null MeshComp Found!"), *Owner->GetActorLabel()));
		return false;
	}
	if (SweepPath.IsEmpty())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner(
			FString::Printf(
				TEXT("[ERROR]%s Create Intersection Failed,Spline Data Is Empty"), *Owner->GetActorLabel()));
		return false;
	}
	
	//临时占位
	TArray<FVector2D> ExtrudeShape =SweepPath;
	if (ExtrudeShape.IsEmpty())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner(
			FString::Printf(
				TEXT("[ERROR]%s Create Intersection Failed,Extrude Is Not Defined"), *Owner->GetActorLabel()));
		return false;
	}

	FGeometryScriptPrimitiveOptions GeometryScriptOptions;
	FTransform ExtrudeMeshTrans = FTransform::Identity;
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSimpleExtrudePolygon(
		MeshComponent->GetDynamicMesh(), GeometryScriptOptions, ExtrudeMeshTrans, ExtrudeShape, 30.0f);
	UGeometryScriptLibrary_MeshNormalsFunctions::AutoRepairNormals(MeshComponent->GetDynamicMesh());
	FGeometryScriptSplitNormalsOptions SplitOptions;
	FGeometryScriptCalculateNormalsOptions CalculateOptions;
	UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(MeshComponent->GetDynamicMesh(), SplitOptions,
																	 CalculateOptions);
	return true;
}
