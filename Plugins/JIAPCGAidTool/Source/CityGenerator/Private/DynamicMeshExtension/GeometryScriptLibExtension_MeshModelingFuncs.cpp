// Fill out your copyright notice in the Description page of Project Settings.


#include "DynamicMeshExtension/GeometryScriptLibExtension_MeshModelingFuncs.h"

#include "Components/BaseDynamicMeshComponent.h"
#include "DynamicMesh/MeshNormals.h"

#define LOCTEXT_NAMESPACE "UGeometryScriptLibrary_MeshModelingFunctions"

UDynamicMesh* UGeometryScriptLibExtension_MeshModelingFuncs::ApplyMeshShrinkVertices(UDynamicMesh* TargetMesh,
	const FGeometryScriptMeshShrinkVerticesOptions& Options, UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs,
		                          LOCTEXT("ApplyMeshShrinkVertices_InvalidInput",
		                                  "ApplyMeshShrinkVertices: TargetMesh is Null"));
		return TargetMesh;
	}

	TargetMesh->EditMesh([&](::FDynamicMesh3& EditMesh)
	{
		// 1. 收集需要处理的顶点
		TSet<int32> VerticesToProcess;
		if (Options.Selection.GetNumSelected() == 0)
		{
			for (int32 Vid : EditMesh.VertexIndicesItr())
			{
				VerticesToProcess.Add(Vid);
			}
		}
		else
		{
			TArray<int32> Triangles;
			Options.Selection.ConvertToMeshIndexArray(EditMesh, Triangles, EGeometryScriptIndexType::Triangle);
			for (int32 Tid : Triangles)
			{
				UE::Geometry::FIndex3i Tri = EditMesh.GetTriangle(Tid);
				VerticesToProcess.Add(Tri.A);
				VerticesToProcess.Add(Tri.B);
				VerticesToProcess.Add(Tri.C);
			}
		}

		// 2. 计算顶点法线
		UE::Geometry::FMeshNormals Normals(&EditMesh);
		Normals.ComputeVertexNormals();

		// 3. 执行内缩
		const float Offset = FMath::Max(0.f, Options.Distance);
		for (int32 Vid : VerticesToProcess)
		{
			if (!EditMesh.IsVertex(Vid)) continue;
			FVector3d Pos = EditMesh.GetVertex(Vid);
			FVector3d Normal = Normals[Vid];
			EditMesh.SetVertex(Vid, Pos - Normal * Offset);
		}
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	return TargetMesh;
}

#undef LOCTEXT_NAMESPACE
