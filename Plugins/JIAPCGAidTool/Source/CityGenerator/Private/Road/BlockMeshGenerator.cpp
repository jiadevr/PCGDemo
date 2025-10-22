// Fill out your copyright notice in the Description page of Project Settings.


#include "Road/BlockMeshGenerator.h"

#include "EditorComponentUtilities.h"
#include "NotifyUtilities.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SplineComponent.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshSelectionFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshMaterialFunctions.h"
#include "Kismet/KismetMathLibrary.h"
#include "Subsystems/EditorAssetSubsystem.h"

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
	UGeometryScriptLibrary_MeshMaterialFunctions::SetMaterialIDForMeshSelection(MeshPtr, NewUpFaceSelection, 1);
	//把内部挤压的单独拿出来设置
	UGeometryScriptLibrary_MeshMaterialFunctions::SetMaterialIDForMeshSelection(MeshPtr, UpFaceSelection, 0);

	//其他不可见的面
	FGeometryScriptMeshSelection OtherFaceSelection;
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByNormalAngle(
		MeshPtr, OtherFaceSelection, FVector::UpVector, 1, EGeometryScriptMeshSelectionType::Triangles, true);
	UGeometryScriptLibrary_MeshMaterialFunctions::SetMaterialIDForMeshSelection(MeshPtr, OtherFaceSelection, 2);
	if (Materials.IsEmpty())
	{
		InitialMaterials();
	}
	RefreshMatsOnDynamicMeshComp();
	return true;
}

void UBlockMeshGenerator::SetInnerSplinePoints(
	const TArray<FInterpCurveVector>& InOrderedControlPoints)
{
	ControlPointsOfAmongRoads = InOrderedControlPoints;
	//DrawDebugLines
	if (bDrawVisualDebug)
	{
		FColor LineInIndividualGroup = FColor::MakeRandomSeededColor(GetGlobalIndex());
		FColor LineBetweenGroups = FColor::MakeRandomColor();
		FVector VerticalOffset = FVector::UpVector * 50.0;
		TArray<const FInterpCurvePoint<FVector>*> ConnectedPoints;
		ConnectedPoints.Reserve(ControlPointsOfAmongRoads.Num() * 2);
		for (const FInterpCurve<FVector>& ControlPointsOfSingleRoad : ControlPointsOfAmongRoads)
		{
			ConnectedPoints.Emplace(&ControlPointsOfSingleRoad.Points[0]);
			for (int i = 1; i < ControlPointsOfSingleRoad.Points.Num(); ++i)
			{
				FInterpCurvePoint<FVector> LastPoint = ControlPointsOfSingleRoad.Points[i - 1];
				FInterpCurvePoint<FVector> CurrentPoint = ControlPointsOfSingleRoad.Points[i];
				DrawDebugLine(GetWorld(), LastPoint.OutVal + VerticalOffset, CurrentPoint.OutVal + VerticalOffset,
				              LineInIndividualGroup, true, -1, 0, 50.0f);
			}
			ConnectedPoints.Emplace(&ControlPointsOfSingleRoad.Points.Last());
		}
		for (int32 i = 1; i < ConnectedPoints.Num(); i += 2)
		{
			int32 ConnectTo = (i + 1) % ConnectedPoints.Num();
			DrawDebugLine(GetWorld(), ConnectedPoints[i]->OutVal + VerticalOffset,
			              ConnectedPoints[ConnectTo]->OutVal + VerticalOffset, LineBetweenGroups, true,
			              -1, 0, 50.0f);
		}
	}
}

void UBlockMeshGenerator::GenerateInnerRefSpline()
{
	AActor* Owner = MeshComponent->GetOwner();
	if (nullptr == Owner)
	{
		return;
	}
	UActorComponent* SplineCompTemp = UEditorComponentUtilities::AddComponentInEditor(
		Owner, USplineComponent::StaticClass());
	if (nullptr == SplineCompTemp) { return; }
	RefSpline = Cast<USplineComponent>(SplineCompTemp);
	RefSpline->ClearSplinePoints();
	TArray<const FInterpCurvePoint<FVector>*> ControlPoints;
	for (FInterpCurve<FVector>& ControlPointsOfSingleRoad : ControlPointsOfAmongRoads)
	{
		AdjustTangentValueInline(ControlPointsOfSingleRoad);
		for (int i = 0; i < ControlPointsOfSingleRoad.Points.Num(); ++i)
		{
			ControlPoints.Emplace(&ControlPointsOfSingleRoad.Points[i]);
		}
		//理论上不存在
		if (1 == ControlPointsOfSingleRoad.Points.Num())
		{
			continue;
		}
	}
	for (int i = 0; i < ControlPoints.Num(); ++i)
	{
		FVector LocationInLS = UKismetMathLibrary::InverseTransformLocation(
			Owner->GetTransform(), ControlPoints[i]->OutVal);
		FVector ArriveTangentInLS = UKismetMathLibrary::InverseTransformDirection(
			Owner->GetTransform(), ControlPoints[i]->ArriveTangent);
		FVector LeaveTangentInLS = UKismetMathLibrary::InverseTransformDirection(
			Owner->GetTransform(), ControlPoints[i]->LeaveTangent);
		ESplinePointType::Type PointType = ConvertInterpCurveModeToSplinePointType(ControlPoints[i]->InterpMode);
		if (PointType == ESplinePointType::Curve || PointType == ESplinePointType::CurveClamped)
		{
			PointType = ESplinePointType::CurveCustomTangent;
		}
		FSplinePoint SplinePoint(i, LocationInLS,
		                         ArriveTangentInLS, LeaveTangentInLS, FRotator(0),
		                         FVector(1), PointType);
		RefSpline->AddPoint(SplinePoint, false);
	}
	/*for (int i = 0; i < RefSpline->GetNumberOfSplinePoints(); ++i)
	{
		FVector ArriveTangent = RefSpline->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
		FVector LeaveTangent = RefSpline->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
		UE_LOG(LogTemp, Warning, TEXT("Point %d - ArriveTangent: %s, LeaveTangent: %s"),
		       i, *ArriveTangent.ToString(), *LeaveTangent.ToString());
	}
	UE_LOG(LogTemp, Warning, TEXT("_______________________________________________"))*/
	RefSpline->SetClosedLoop(true, false);
	RefSpline->UpdateSpline();
	/*for (int i = 0; i < RefSpline->GetNumberOfSplinePoints(); ++i)
	{
		FVector ArriveTangent = RefSpline->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
		FVector LeaveTangent = RefSpline->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
		UE_LOG(LogTemp, Warning, TEXT("Point %d - ArriveTangent: %s, LeaveTangent: %s"),
		       i, *ArriveTangent.ToString(), *LeaveTangent.ToString());
	}*/
}

void UBlockMeshGenerator::RefreshMatsOnDynamicMeshComp()
{
	if (!Materials.IsEmpty() && MeshComponent.IsValid())
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

void UBlockMeshGenerator::InitialMaterials()
{
	Materials.Empty();
	UEditorAssetSubsystem* AssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	if (!AssetSubsystem)
	{
		return;
	}
	for (int i = 0; i < MaterialsPath.Num(); ++i)
	{
		const FString* MaterialPath = &MaterialsPath[i];
		if (!AssetSubsystem->DoesAssetExist(*MaterialPath))
		{
			if (AssetSubsystem->DoesAssetExist(*BackupMaterialsPath[i]))
			{
				MaterialPath = &BackupMaterialsPath[i];
			}
			else
			{
				MaterialPath = nullptr;
			}
		}
		if (nullptr == MaterialPath)
		{
			Materials.Emplace(nullptr);
			continue;
		}
		UObject* TargetMatAsset = AssetSubsystem->LoadAsset(*MaterialPath);
		if (TargetMatAsset)
		{
			Materials.Emplace(Cast<UMaterialInterface>(TargetMatAsset));
		}
	}
	Materials.Emplace(nullptr);
}

void UBlockMeshGenerator::AdjustTangentValueInline(FInterpCurve<FVector>& PointGroup)
{
	if (PointGroup.Points.Num() <= 2)
	{
		return;
	}
	//首尾不涉及Tangent问题
	for (int32 i = 1; i < PointGroup.Points.Num() - 1; ++i)
	{
		const FVector& LocWS = PointGroup.Points[i].OutVal;
		DrawDebugSphere(GetWorld(), LocWS, 100.0f, 10, FColor::Red, true, -1, 0, 10.0f);
		const FVector& PreLocWS = PointGroup.Points[i - 1].OutVal;
		const FVector& NextLocWS = PointGroup.Points[i + 1].OutVal;
		const double DisSquaredToNeighbour = FMath::Min(FVector::DistSquared(LocWS, PreLocWS),
		                                                FVector::DistSquared(LocWS, NextLocWS));
		//使用Curve类型时ArriveTangent和LeaveTangent可以不同，根据SplineControlPoint管的是后一段的思路使用LeaveTangent
		FVector& Tangent = PointGroup.Points[i].LeaveTangent;
		if (Tangent.SquaredLength() > DisSquaredToNeighbour && (PointGroup.Points[i].InterpMode == CIM_CurveUser ||
			PointGroup.Points[i].InterpMode == CIM_CurveAuto || PointGroup.Points[i].InterpMode ==
			CIM_CurveAutoClamped))
		{
			PointGroup.Points[i].InterpMode = CIM_CurveUser;
			float OriginalDistance = Tangent.Size();
			Tangent = Tangent.GetSafeNormal() * 0.8 * FMath::Sqrt(DisSquaredToNeighbour);
			PointGroup.Points[i].ArriveTangent = Tangent;
			UE_LOG(LogTemp, Display,
			       TEXT("Index [%d] At [%s] Tangent Value Was Clamped From %f To %f,newTangentValue %s"), i,
			       *LocWS.ToString(), OriginalDistance, Tangent.Size(), *Tangent.ToString());
		}
	}
}
