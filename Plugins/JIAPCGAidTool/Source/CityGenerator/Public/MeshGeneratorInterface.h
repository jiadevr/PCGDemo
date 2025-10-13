// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MeshGeneratorInterface.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UMeshGeneratorInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class CITYGENERATOR_API IMeshGeneratorInterface
{
	GENERATED_BODY()

public:
	virtual void SetMeshComponent(class UDynamicMeshComponent* InMeshComponent) =0;

	virtual bool GenerateMesh() =0;

	int32 GetGlobalIndex() const { return GlobalIndex; };

	void SetDrawVisualDebug(bool bDrawDebug) { bDrawVisualDebug = bDrawDebug; };

protected:
	TWeakObjectPtr<UDynamicMeshComponent> MeshComponent;

	int32 GlobalIndex = 0;

	bool bDrawVisualDebug = false;
};
