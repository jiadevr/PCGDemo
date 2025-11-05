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
#include "GeometryScript/MeshPolygroupFunctions.h"
#include "GeometryScript/MeshRepairFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"
#include "Kismet/KismetMathLibrary.h"
#include "Road/RoadGeometryUtilities.h"
#include "Subsystems/EditorAssetSubsystem.h"

int32 UBlockMeshGenerator::BlockGlobalIndex = -1;
const int32 UBlockMeshGenerator::InnerAreaGroupIndex = 0;
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
		ExtrudePath.Emplace(UKismetMathLibrary::InverseTransformLocation(OwnerTransform, PathPoint));
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
	if (ExtrudePath.IsEmpty())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner(
			FString::Printf(
				TEXT("[ERROR]%s Create Block Failed,Spline Data Is Empty"), *Owner->GetActorLabel()));
		return false;
	}

	const TArray<FVector2D>& ExtrudeShape = ExtrudePath;
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
	//使用AppendDelaunayTriangulation2D方法创建面，提升成面质量
	TArray<FIntPoint> BorderEdges;
	GenerateBorderEdgeArray(ExtrudePath, BorderEdges);
	FGeometryScriptConstrainedDelaunayTriangulationOptions TriangulationOptions;
	TriangulationOptions.ConstrainedEdgesFillMode = EGeometryScriptPolygonFillMode::Solid;
	TArray<int32> PositionsToVIDs;
	bool bHasDuplicateVertices = false;
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendDelaunayTriangulation2D(
		MeshPtr, GeometryScriptOptions, ExtrudeMeshTrans, ExtrudeShape, BorderEdges, TriangulationOptions,
		PositionsToVIDs, bHasDuplicateVertices);

	FGeometryScriptMeshLinearExtrudeOptions ExtrudeOptions;
	//这个值比实际需要的大，因为后边优化阈值是50，太小厚度会被消除
	ExtrudeOptions.Distance = 100.0f;
	FGeometryScriptMeshSelection Selection;
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshLinearExtrudeFaces(MeshPtr, ExtrudeOptions, Selection);

	UGeometryScriptLibrary_MeshNormalsFunctions::AutoRepairNormals(MeshPtr);
	FGeometryScriptSplitNormalsOptions SplitOptions;
	FGeometryScriptCalculateNormalsOptions CalculateOptions;
	UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(MeshPtr, SplitOptions,
	                                                                 CalculateOptions);
	//设置统一PolyGroupID
	FGeometryScriptMeshSelection EntireSelection;
	UGeometryScriptLibrary_MeshSelectionFunctions::CreateSelectAllMeshSelection(MeshPtr, EntireSelection);
	FGeometryScriptGroupLayer DefaultGroupLayer;
	int32 IDOut;
	UGeometryScriptLibrary_MeshPolygroupFunctions::SetPolygroupForMeshSelection(
		MeshPtr, DefaultGroupLayer, EntireSelection, IDOut, 2);

	FGeometryScriptMeshSelection UpFaceSelection;
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByNormalAngle(MeshPtr, UpFaceSelection);
	FGeometryScriptMeshInsetOutsetFacesOptions InsetOptions;
	InsetOptions.Distance = 300.0f;
	InsetOptions.Softness = 0.0f;
	InsetOptions.AreaMode = EGeometryScriptPolyOperationArea::EntireSelection;
	FGeometryScriptMeshEditPolygroupOptions SplitPolyGroupOptions;
	/*//设置的Group对象是挤压的那个面，也就是原选择面，而不是内部挤压新生成的周边面
	SplitPolyGroupOptions.GroupMode = EGeometryScriptMeshEditPolygroupMode::AutoGenerateNew;
	SplitPolyGroupOptions.ConstantGroup = 1;*/
	InsetOptions.GroupOptions = SplitPolyGroupOptions;
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshInsetOutsetFaces(MeshPtr, InsetOptions, UpFaceSelection);
	UGeometryScriptLibrary_MeshMaterialFunctions::EnableMaterialIDs(MeshPtr);
	//先给所有顶面都设置
	FGeometryScriptMeshSelection NewUpFaceSelection;
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByNormalAngle(MeshPtr, NewUpFaceSelection);
	UGeometryScriptLibrary_MeshMaterialFunctions::SetMaterialIDForMeshSelection(MeshPtr, NewUpFaceSelection, 1);
	UGeometryScriptLibrary_MeshPolygroupFunctions::SetPolygroupForMeshSelection(
		MeshPtr, DefaultGroupLayer, NewUpFaceSelection, IDOut, 1);
	//把内部挤压的单独拿出来设置
	UGeometryScriptLibrary_MeshMaterialFunctions::SetMaterialIDForMeshSelection(
		MeshPtr, UpFaceSelection, InnerAreaGroupIndex);
	UGeometryScriptLibrary_MeshPolygroupFunctions::SetPolygroupForMeshSelection(
		MeshPtr, DefaultGroupLayer, UpFaceSelection, IDOut, InnerAreaGroupIndex);

	//其他不可见的面
	FGeometryScriptMeshSelection OtherFaceSelection;
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByNormalAngle(
		MeshPtr, OtherFaceSelection, FVector::UpVector, 1, EGeometryScriptMeshSelectionType::Triangles, true);
	UGeometryScriptLibrary_MeshMaterialFunctions::SetMaterialIDForMeshSelection(MeshPtr, OtherFaceSelection, 2);
	if (Materials.IsEmpty())
	{
		InitialMaterials();
	}
	//修正拐角处交叉
	//@TODO:这个方法只能处理部分，后续还是得考虑自定义
	FGeometryScriptDegenerateTriangleOptions DegenerateTriangleOptions;
	DegenerateTriangleOptions.MinEdgeLength = 50.0f;
	DegenerateTriangleOptions.MinTriangleArea = 50.0f;
	UGeometryScriptLibrary_MeshRepairFunctions::RepairMeshDegenerateGeometry(MeshPtr, DegenerateTriangleOptions);

	//缩放回原始高度
	UGeometryScriptLibrary_MeshTransformFunctions::ScaleMesh(MeshPtr, FVector(1.0f, 1.0f, 0.3f));
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

	for (int32 i = 0; i < ControlPointsOfAmongRoads.Num(); ++i)
	{
		AdjustTangentValueInline(ControlPointsOfAmongRoads[i]);
		//样条线逆时针排序，
		for (int j = 0; j < ControlPointsOfAmongRoads[i].Points.Num(); ++j)
		{
			ControlPoints.Emplace(&ControlPointsOfAmongRoads[i].Points[j]);
		}
		//理论上不存在
		if (1 == ControlPointsOfAmongRoads[i].Points.Num())
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
	RefSpline->SetClosedLoop(true, false);
	RefSpline->UpdateSpline();
}

void UBlockMeshGenerator::ExtractLinearContourOfInnerArea()
{
	AActor* Owner = MeshComponent->GetOwner();
	if (nullptr == Owner)
	{
		return;
	}
	if (!MeshComponent.IsValid())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner(
			FString::Printf(
				TEXT("[ERROR]%s Create Block Failed,Null MeshComp Found!"), *Owner->GetActorLabel()));
		return;
	}
	TArray<FVector> InnerBorder = GetInnerAreaBorder();
	FTransform OwnerTransform = Owner->GetTransform();
	/*for (const FVector& BorderPoint : InnerBorder)
	{
		FVector VertexInWS = UKismetMathLibrary::TransformLocation(OwnerTransform, BorderPoint);
		DrawDebugSphere(GetWorld(), VertexInWS, 50.0f, 8, FColor::Red, true, -1, 0, 5.0f);
	}*/
	URoadGeometryUtilities::SimplifySplinePointsInline(InnerBorder, true);
	/*
	for (const FVector& BorderPoint : InnerBorder)
	{
		FVector VertexInWS = UKismetMathLibrary::TransformLocation(OwnerTransform, BorderPoint);
		DrawDebugSphere(GetWorld(), VertexInWS + FVector::UpVector * 20.0f, 50.0f, 8, FColor::Blue, true, -1, 0, 5.0f);
	}*/
	TArray<FSplinePoint> SimplifiedPoints;
	for (int32 i = 0; i < InnerBorder.Num(); ++i)
	{
		SimplifiedPoints.Emplace(i, InnerBorder[i], ESplinePointType::Linear);
	}
	if (SimplifiedPoints.IsEmpty())
	{
		return;
	}
	UActorComponent* SplineCompTemp = UEditorComponentUtilities::AddComponentInEditor(
		Owner, USplineComponent::StaticClass());
	if (nullptr == SplineCompTemp) { return; }
	RefSpline = Cast<USplineComponent>(SplineCompTemp);
	RefSpline->ClearSplinePoints();
	RefSpline->AddPoints(SimplifiedPoints, false);
	RefSpline->SetClosedLoop(true, false);
	RefSpline->UpdateSpline();
	URoadGeometryUtilities::ResolveTwistySplineSegments(RefSpline, true);
}

void UBlockMeshGenerator::RefreshMatsOnDynamicMeshComp()
{
	if (!Materials.IsEmpty() && MeshComponent.IsValid())
	{
		MeshComponent->ConfigureMaterialSet(Materials);
	}
}

void UBlockMeshGenerator::GenerateBorderEdgeArray(const TArray<FVector2D>& InBorderPoints,
                                                  TArray<FIntPoint>& OutBorderEdgeArray)
{
	OutBorderEdgeArray.Reset();
	int32 PointCount = InBorderPoints.Num();
	if (PointCount == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Empty PointArray"))
		return;
	}
	OutBorderEdgeArray.Reserve(PointCount);
	for (int i = 0; i < PointCount; ++i)
	{
		FIntPoint EdgeToNext{i, (i + 1) % PointCount};
		OutBorderEdgeArray.Add(EdgeToNext);
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
			Materials.Add(Cast<UMaterialInterface>(TargetMatAsset));
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

TArray<FVector> UBlockMeshGenerator::GetInnerAreaBorder()
{
	TArray<FVector> BorderPoints;
	if (!MeshComponent.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Find Null DynamicMeshComp,Return Empty"))
		return BorderPoints;
	}
	UDynamicMesh* BlockMesh = MeshComponent->GetDynamicMesh();
	if (nullptr == BlockMesh)
	{
		return BorderPoints;
	}
	int32 SelectElemCount = 0;

	FGeometryScriptGroupLayer GroupLayer;
	FGeometryScriptMeshSelection InnerFaceSelection;
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByPolygroup(
		BlockMesh, GroupLayer, InnerAreaGroupIndex, InnerFaceSelection);
	SelectElemCount = InnerFaceSelection.GetNumSelected();
	UE_LOG(LogTemp, Display, TEXT("Select Triangle %d"), SelectElemCount);
	FGeometryScriptMeshSelection BoundaryEdges;
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectSelectionBoundaryEdges(
		BlockMesh, InnerFaceSelection, BoundaryEdges);
	SelectElemCount = BoundaryEdges.GetNumSelected();
	UE_LOG(LogTemp, Display, TEXT("Select Edge %d"), SelectElemCount);
	TArray<int32> EdgeIDs;
	EGeometryScriptMeshSelectionType SelectionType;
	UGeometryScriptLibrary_MeshSelectionFunctions::ConvertMeshSelectionToIndexArray(
		BlockMesh, BoundaryEdges, EdgeIDs, SelectionType);
	//使用DelaunayTriangulation算法构建边界之后返回的是按三角形顺序，但因为传进去的时候有序，这里依然可以使用有序特性把值取回来
	EdgeIDs.Sort();
	SelectElemCount = EdgeIDs.Num();
	UE_LOG(LogTemp, Display, TEXT("Select EdgeIndex %d"), SelectElemCount);
	FDynamicMesh3& Mesh3 = BlockMesh->GetMeshRef();
	for (int32 i = 0; i < EdgeIDs.Num() - 1; ++i)
	{
		FVector3d Start;
		FVector3d End;
		Mesh3.GetEdgeV(EdgeIDs[i], Start, End);
		BorderPoints.Add(Start);
	}
	return BorderPoints;
}
