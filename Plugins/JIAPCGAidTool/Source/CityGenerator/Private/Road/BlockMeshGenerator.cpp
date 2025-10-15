// Fill out your copyright notice in the Description page of Project Settings.


#include "Road/BlockMeshGenerator.h"

#include "NotifyUtilities.h"
#include "Components/DynamicMeshComponent.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshSelectionFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshMaterialFunctions.h"
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
				TEXT("[ERROR]%s Create Block Failed,Null MeshComp Found!"), *Owner->GetActorLabel()));
		return false;
	}
	if (SweepPath.IsEmpty())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner(
			FString::Printf(
				TEXT("[ERROR]%s Create Block Failed,Spline Data Is Empty"), *Owner->GetActorLabel()));
		return false;
	}

	TArray<FVector2D> ExtrudeShape = SweepPath;
	if (ExtrudeShape.IsEmpty())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner(
			FString::Printf(
				TEXT("[ERROR]%s Create Block Failed,Extrude Is Not Defined"), *Owner->GetActorLabel()));
		return false;
	}

	UDynamicMesh* MeshPtr = MeshComponent->GetDynamicMesh();
	FGeometryScriptPrimitiveOptions GeometryScriptOptions;
	FTransform ExtrudeMeshTrans = FTransform::Identity;
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSimpleExtrudePolygon(
		MeshPtr, GeometryScriptOptions, ExtrudeMeshTrans, ExtrudeShape, 30.0f);


	UGeometryScriptLibrary_MeshNormalsFunctions::AutoRepairNormals(MeshPtr);
	FGeometryScriptSplitNormalsOptions SplitOptions;
	FGeometryScriptCalculateNormalsOptions CalculateOptions;
	UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(MeshPtr, SplitOptions,
	                                                                 CalculateOptions);

	FGeometryScriptMeshSelection UpFaceSelection;
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByNormalAngle(MeshPtr, UpFaceSelection);
	FGeometryScriptMeshInsetOutsetFacesOptions InsetOptions;
	InsetOptions.Distance = 400.0f;
	InsetOptions.Softness = 100.0f;
	InsetOptions.AreaMode = EGeometryScriptPolyOperationArea::EntireSelection;
	FGeometryScriptMeshEditPolygroupOptions SplitPolyGroupOptions;
	//设置的Group对象是挤压的那个面，也就是原选择面，而不是内部挤压新生成的周边面
	SplitPolyGroupOptions.GroupMode = EGeometryScriptMeshEditPolygroupMode::AutoGenerateNew;
	SplitPolyGroupOptions.ConstantGroup = 1;
	InsetOptions.GroupOptions = SplitPolyGroupOptions;
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshInsetOutsetFaces(MeshPtr, InsetOptions, UpFaceSelection);
	UGeometryScriptLibrary_MeshMaterialFunctions::EnableMaterialIDs(MeshPtr);
	//先给所有顶面都设置
	FGeometryScriptMeshSelection NewUpFaceSelection;
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByNormalAngle(MeshPtr, NewUpFaceSelection);
	UGeometryScriptLibrary_MeshMaterialFunctions::SetMaterialIDForMeshSelection(MeshPtr, NewUpFaceSelection, 2);
	//把内部挤压的单独拿出来设置
	UGeometryScriptLibrary_MeshMaterialFunctions::SetMaterialIDForMeshSelection(MeshPtr, UpFaceSelection, 1);
	//其他不可见的面
	FGeometryScriptMeshSelection OtherFaceSelection;
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByNormalAngle(
		MeshPtr, OtherFaceSelection, FVector::UpVector, 1, EGeometryScriptMeshSelectionType::Triangles, true);
	UGeometryScriptLibrary_MeshMaterialFunctions::SetMaterialIDForMeshSelection(MeshPtr, OtherFaceSelection, 0);
	return true;
}

void UBlockMeshGenerator::RefreshMatsOnDynamicMeshComp()
{
	if (!Materials.IsEmpty())
	{
		MeshComponent->ConfigureMaterialSet(Materials);
	}
}
#if WITH_EDITOR
void UBlockMeshGenerator::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property)
	{
		FName PropertyName = PropertyChangedEvent.Property->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UBlockMeshGenerator, Materials))
		{
			RefreshMatsOnDynamicMeshComp();
		}
	}
}
#endif
