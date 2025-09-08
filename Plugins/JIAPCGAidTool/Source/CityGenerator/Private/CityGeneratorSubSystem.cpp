// Fill out your copyright notice in the Description page of Project Settings.


#include "CityGeneratorSubSystem.h"

#include "EditorComponentUtilities.h"
#include "NotifyUtilities.h"
#include "RoadGeneratorSubsystem.h"
#include "SubobjectDataSubsystem.h"
#include "SubobjectDataHandle.h"
#include  "SubobjectData.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SplineComponent.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Subsystems/UnrealEditorSubsystem.h"

#pragma region  Base
void UCityGeneratorSubSystem::CollectAllSplines(const FName OptionalActorTag/*=""*/, const FName OptionalCompTag/*=""*/)
{
	TArray<AActor*> PotentialActors;
	if (OptionalActorTag.IsNone())
	{
		UGameplayStatics::GetAllActorsOfClass(GetEditorContext(), AActor::StaticClass(), PotentialActors);
	}
	else
	{
		UGameplayStatics::GetAllActorsOfClassWithTag(GetEditorContext(), AActor::StaticClass(), OptionalActorTag,
		                                             PotentialActors);
	}
	if (PotentialActors.IsEmpty())
	{
		return;
	}
	CityGeneratorSplineArray.Empty();
	for (const auto PotentialActor : PotentialActors)
	{
		TArray<UActorComponent*> ComponentsByClass;
		if (OptionalCompTag.IsNone())
		{
			PotentialActor->GetComponents(USplineComponent::StaticClass(), ComponentsByClass);
		}
		else
		{
			ComponentsByClass = PotentialActor->GetComponentsByTag(USplineComponent::StaticClass(), OptionalCompTag);
		}
		if (ComponentsByClass.IsEmpty())
		{
			continue;
		}
		for (UActorComponent* Component : ComponentsByClass)
		{
			USplineComponent* SplineComp = Cast<USplineComponent>(Component);
			if (nullptr != SplineComp)
			{
				CityGeneratorSplineArray.Emplace(SplineComp);
			}
		}
	}
	FString CountStr = FString::Printf(TEXT("Get %d SplineComps"), CityGeneratorSplineArray.Num());
	UNotifyUtilities::ShowPopupMsgAtCorner(CountStr);
	UE_LOG(LogTemp, Display, TEXT("%s"), *CountStr);
}

void UCityGeneratorSubSystem::SerializeSplines(const FString& FileName, const FString& FilePath, bool bSaveActorTag,
                                               bool bSaveCompTag, bool bForceRecollect)
{
	//验证是否包含数据
	if (bForceRecollect)
	{
		CollectAllSplines();
	}
	if (CityGeneratorSplineArray.IsEmpty())
	{
		UNotifyUtilities::ShowMsgDialog(EAppMsgType::Ok,
		                                TEXT("CityGeneratorSplineArray Is Empty,Please Collect First!"));
		return;
	}
	//验证路径有效性
	FString TargetDir = FilePath.IsEmpty() ? FPaths::ProjectSavedDir() : FilePath;
	UE_LOG(LogTemp, Display, TEXT("%s"), *TargetDir);
	if (!TargetDir.EndsWith("/"))
	{
		TargetDir.AppendChar('/');
	}
	TargetDir += FileName;
	TargetDir += ".json";
	bool bFileExited = false;
	if (FPaths::FileExists(TargetDir))
	{
		bFileExited = true;
		if (EAppReturnType::Yes == UNotifyUtilities::ShowMsgDialog(EAppMsgType::YesNoCancel,
		                                                           FString::Printf(
			                                                           TEXT(
				                                                           "%s Already Existed In Given Path, Press \"Yes\" To Override It"),
			                                                           *FileName)))
		{
			bFileExited = false;
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			PlatformFile.DeleteFile(*TargetDir);
		}
	}
	if (bFileExited)
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Save Canceled!");
		return;
	}
	//进行数据序列化
	TSharedPtr<FJsonObject> SceneSplinesData = MakeShareable(new FJsonObject());
	/*整体结构为
	 Scene{
		Splines[
			SingleSpline{
				Points[
					SinglePoint{}
				]
			}
		]
	 }
	 */
	//场景Spline通用数据
	SceneSplinesData->SetStringField(TEXT("CoordSpace"), "Local");
	//场景中所有Spline数据
	TArray<TSharedPtr<FJsonValue>> SplineDataArray;
	int32 SplineCounter = 0;
	for (const TWeakObjectPtr<USplineComponent> SingleSpline : CityGeneratorSplineArray)
	{
		if (!SingleSpline.IsValid()) { continue; }
		TStrongObjectPtr<USplineComponent> TargetSpline = SingleSpline.Pin();
		AActor* SplineOwner = TargetSpline->GetOwner();

		//单个Spline数据
		TSharedPtr<FJsonObject> SingleSplineData(new FJsonObject());
		//SplineOwner信息输入
		if (nullptr == SplineOwner) { continue; }
		SingleSplineData->SetStringField(TEXT("OwnerName"), SplineOwner->GetActorNameOrLabel());
		//用于判定是不是同一个Actor身上的多个SplineComp
		SingleSplineData->SetStringField(TEXT("OwnerActorID"), SplineOwner->GetActorGuid().ToString());
		SingleSplineData->SetNumberField(TEXT("Index"), SplineCounter);
		//ActorTag和CompTag
		if (bSaveActorTag && !SplineOwner->Tags.IsEmpty())
		{
			TArray<TSharedPtr<FJsonValue>> ActorTags;
			int32 ActorTagCounter = 0;
			for (const FName& ActorTag : SplineOwner->Tags)
			{
				TSharedPtr<FJsonObject> SingleTag;
				FString TagName = "Tag" + ActorTagCounter;
				SingleTag->SetStringField(TagName, ActorTag.ToString());
				ActorTags.Emplace(MakeShareable(new FJsonValueObject(SingleTag)));
				ActorTagCounter++;
			}
			SingleSplineData->SetArrayField(TEXT("ActorTags"), ActorTags);
		}
		if (bSaveCompTag && !TargetSpline->ComponentTags.IsEmpty())
		{
			TArray<TSharedPtr<FJsonValue>> ComponentTags;
			int32 CompTagCounter = 0;
			for (const FName& CompTag : TargetSpline->ComponentTags)
			{
				TSharedPtr<FJsonObject> SingleTag;
				FString TagName = "Tag" + CompTagCounter;
				SingleTag->SetStringField(TagName, CompTag.ToString());
				ComponentTags.Emplace(MakeShareable(new FJsonValueObject(SingleTag)));
				CompTagCounter++;
			}
			SingleSplineData->SetArrayField(TEXT("CompTags"), ComponentTags);
		}
		//ActorTransform
		FTransform OwnerTransform = SplineOwner->GetActorTransform();
		SingleSplineData->SetNumberField(TEXT("Location.X"), OwnerTransform.GetLocation().X);
		SingleSplineData->SetNumberField(TEXT("Location.Y"), OwnerTransform.GetLocation().Y);
		SingleSplineData->SetNumberField(TEXT("Location.Z"), OwnerTransform.GetLocation().Z);

		SingleSplineData->SetNumberField(TEXT("Rotation.X"), OwnerTransform.GetRotation().X);
		SingleSplineData->SetNumberField(TEXT("Rotation.Y"), OwnerTransform.GetRotation().Y);
		SingleSplineData->SetNumberField(TEXT("Rotation.Z"), OwnerTransform.GetRotation().Z);
		SingleSplineData->SetNumberField(TEXT("Rotation.W"), OwnerTransform.GetRotation().W);

		SingleSplineData->SetNumberField(TEXT("Scale.X"), OwnerTransform.GetScale3D().X);
		SingleSplineData->SetNumberField(TEXT("Scale.Y"), OwnerTransform.GetScale3D().Y);
		SingleSplineData->SetNumberField(TEXT("Scale.Z"), OwnerTransform.GetScale3D().Z);
		//SplineControlPoint信息
		TArray<TSharedPtr<FJsonValue>> SingleSplinePointsArray;
		int32 ControlPointCount = TargetSpline->GetNumberOfSplinePoints();
		TArray<FVector> PointsLoc;
		PointsLoc.SetNum(ControlPointCount);
		TArray<FVector> PointsTan;
		PointsTan.SetNum(ControlPointCount);
		TArray<FRotator> PointsRot;
		PointsRot.SetNum(ControlPointCount);


		for (int i = 0; i < ControlPointCount; ++i)
		{
			TargetSpline->GetLocationAndTangentAtSplinePoint(i, PointsLoc[i], PointsTan[i],
			                                                 ESplineCoordinateSpace::Local);
			PointsRot[i] = TargetSpline->GetRotationAtSplinePoint(i, ESplineCoordinateSpace::Local);
		}
		for (int i = 0; i < PointsLoc.Num(); ++i)
		{
			TSharedPtr<FJsonObject> SinglePointData(new FJsonObject());
			int32 PointType = TargetSpline->GetSplinePointType(i);
			SinglePointData->SetNumberField(TEXT("PointType"), PointType);

			SinglePointData->SetNumberField(TEXT("Location.X"), PointsLoc[i].X);
			SinglePointData->SetNumberField(TEXT("Location.Y"), PointsLoc[i].Y);
			SinglePointData->SetNumberField(TEXT("Location.Z"), PointsLoc[i].Z);

			SinglePointData->SetNumberField(TEXT("Tangent.X"), PointsTan[i].X);
			SinglePointData->SetNumberField(TEXT("Tangent.Y"), PointsTan[i].Y);
			SinglePointData->SetNumberField(TEXT("Tangent.Z"), PointsTan[i].Z);

			SinglePointData->SetNumberField(TEXT("Rotation.Yaw"), PointsRot[i].Yaw);
			SinglePointData->SetNumberField(TEXT("Rotation.Pitch"), PointsRot[i].Pitch);
			SinglePointData->SetNumberField(TEXT("Rotation.Roll"), PointsRot[i].Roll);
			SingleSplinePointsArray.Emplace(MakeShareable(new FJsonValueObject(SinglePointData)));
		}
		SingleSplineData->SetArrayField(TEXT("Points"), SingleSplinePointsArray);
		SingleSplineData->SetBoolField(TEXT("bCloseLoop"), TargetSpline->IsClosedLoop());
		SplineDataArray.Emplace(MakeShareable(new FJsonValueObject(SingleSplineData)));
		SplineCounter++;
	}
	SceneSplinesData->SetArrayField(TEXT("Splines"), SplineDataArray);
	FString SerializeStr;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<
		TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&SerializeStr);

	FJsonSerializer::Serialize(SceneSplinesData.ToSharedRef(), JsonWriter);
	UE_LOG(LogTemp, Display, TEXT("Json Res:[%s]"), *SerializeStr);
	FFileHelper::SaveStringToFile(SerializeStr, *TargetDir);
	UNotifyUtilities::ShowPopupMsgAtCorner(FString::Printf(
		TEXT("Save Scene Spline Data Finished!Save %d,Total %d)"), SplineCounter, CityGeneratorSplineArray.Num()));
}

void UCityGeneratorSubSystem::DeserializeSplines(const FString& FileFullPath, bool bTryParseActorTag,
                                                 bool bTryParseCompTag, bool bAutoCollectAfterSpawn)
{
	//判断路径和文件内容合法性
	if (!FPaths::FileExists(FileFullPath))
	{
		UNotifyUtilities::ShowMsgDialog(EAppMsgType::Ok, "File Doesn't Exit!Please Check Path", true);
		UNotifyUtilities::ShowPopupMsgAtCorner("Failed To Load Data");
		return;
	}
	if (!FileFullPath.EndsWith(".json") && !FileFullPath.EndsWith(".JSON"))
	{
		UNotifyUtilities::ShowMsgDialog(EAppMsgType::Ok, "Invalid Format!Please Check Path", true);
		UNotifyUtilities::ShowPopupMsgAtCorner("Failed To Load Data");
		return;
	}
	FString DeserializeStr;
	FFileHelper::LoadFileToString(DeserializeStr, *FileFullPath);
	if (DeserializeStr.IsEmpty())
	{
		UNotifyUtilities::ShowMsgDialog(EAppMsgType::Ok, "Read None Data In Given File", true);
		UNotifyUtilities::ShowPopupMsgAtCorner("Failed To Load Data");
		return;
	}
	//反序列化
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(DeserializeStr);
	TSharedPtr<FJsonObject> SceneSplinesData;
	FJsonSerializer::Deserialize(JsonReader, SceneSplinesData);
	bool bIsLocalCoord = true;
	SceneSplinesData->TryGetBoolField(TEXT("CoordSpace"), bIsLocalCoord);
	//场景中的Spline数据
	TArray<TSharedPtr<FJsonValue>> SplineDataArray;
	//需要一个指向数组的指针
	const TArray<TSharedPtr<FJsonValue>>* SplineDataArrayPtr = &SplineDataArray;
	bool bGetDataSuccess = SceneSplinesData->TryGetArrayField(TEXT("Splines"), SplineDataArrayPtr);
	if (!bGetDataSuccess || SplineDataArrayPtr->IsEmpty())
	{
		UNotifyUtilities::ShowMsgDialog(EAppMsgType::Ok, "Read None Data In Given File", true);
		UNotifyUtilities::ShowPopupMsgAtCorner("Failed To Load Data");
		return;
	}
	//读到了数据开始逐个解析
	TMap<FGuid, TObjectPtr<AActor>> SpawnedActors;
	for (const TSharedPtr<FJsonValue>& SingleSplineDataValue : *SplineDataArrayPtr)
	{
		const TSharedPtr<FJsonObject>& SingleSplineData = SingleSplineDataValue->AsObject();
		FString OwnerActorName = "";
		SingleSplineData->TryGetStringField(TEXT("OwnerName"), OwnerActorName);
		FString OwnerActorGuidStr = "";
		SingleSplineData->TryGetStringField(TEXT("OwnerActorID"), OwnerActorGuidStr);
		FGuid OwnerActorGuid = FGuid(OwnerActorGuidStr);
		//解析Tag
		TArray<FName> ActorTags;
		if (bTryParseActorTag && SingleSplineData->HasField(TEXT("ActorTags")))
		{
			TArray<TSharedPtr<FJsonValue>> ActorTagsJson = SingleSplineData->GetArrayField(TEXT("ActorTags"));
			int32 ActorTagCounter = 0;
			for (const TSharedPtr<FJsonValue>& SingleTag : ActorTagsJson)
			{
				FString TagName = "Tag" + ActorTagCounter;
				ActorTags.Emplace(SingleTag->AsObject()->GetStringField(TagName));
				ActorTagCounter++;
			}
		}
		TArray<FName> SplineCompTags;
		if (bTryParseCompTag && SingleSplineData->HasField(TEXT("CompTags")))
		{
			TArray<TSharedPtr<FJsonValue>> CompTagsJson = SingleSplineData->GetArrayField(TEXT("CompTags"));
			int32 CompTagCounter = 0;
			for (const TSharedPtr<FJsonValue>& SingleTag : CompTagsJson)
			{
				FString TagName = "Tag" + CompTagCounter;
				SplineCompTags.Emplace(SingleTag->AsObject()->GetStringField(TagName));
				CompTagCounter++;
			}
		}
		//解析OwnerTransform
		FVector OwnerLocation;
		SingleSplineData->TryGetNumberField(TEXT("Location.X"), OwnerLocation.X);
		SingleSplineData->TryGetNumberField(TEXT("Location.Y"), OwnerLocation.Y);
		SingleSplineData->TryGetNumberField(TEXT("Location.Z"), OwnerLocation.Z);
		FQuat OwnerRotation;
		SingleSplineData->TryGetNumberField(TEXT("Rotation.X"), OwnerRotation.X);
		SingleSplineData->TryGetNumberField(TEXT("Rotation.Y"), OwnerRotation.Y);
		SingleSplineData->TryGetNumberField(TEXT("Rotation.Z"), OwnerRotation.Z);
		SingleSplineData->TryGetNumberField(TEXT("Rotation.W"), OwnerRotation.W);
		FVector OwnerScale;
		SingleSplineData->TryGetNumberField(TEXT("Scale.X"), OwnerScale.X);
		SingleSplineData->TryGetNumberField(TEXT("Scale.Y"), OwnerScale.Y);
		SingleSplineData->TryGetNumberField(TEXT("Scale.Z"), OwnerScale.Z);
		FTransform OwnerTransform;
		OwnerTransform.SetLocation(OwnerLocation);
		OwnerTransform.SetRotation(OwnerRotation);
		OwnerTransform.SetScale3D(OwnerScale);
		//解析点信息
		TArray<int32> PointsType;
		TArray<FVector> PointsLoc;
		TArray<FVector> PointsTan;
		TArray<FRotator> PointsRot;

		TArray<TSharedPtr<FJsonValue>> SingleSplinePointsArray;
		const TArray<TSharedPtr<FJsonValue>>* SingleSplinePointsArrayPtr = &SingleSplinePointsArray;
		bool bGetControlPointSuccess = SingleSplineData->TryGetArrayField(TEXT("Points"), SingleSplinePointsArrayPtr);
		//没有解析到点信息
		if (!bGetControlPointSuccess || SingleSplinePointsArrayPtr->IsEmpty())
		{
			UNotifyUtilities::ShowMsgDialog(EAppMsgType::Ok, "Read None Data In Given File", true);
			UNotifyUtilities::ShowPopupMsgAtCorner("Failed To Load Data");
			continue;
		}
		for (const TSharedPtr<FJsonValue>& SinglePointDataValue : *SingleSplinePointsArrayPtr)
		{
			const TSharedPtr<FJsonObject>& SinglePointData = SinglePointDataValue->AsObject();
			int32 PointType = 0;
			SinglePointData->TryGetNumberField(TEXT("PointType"), PointType);
			FVector PointLoc;
			SinglePointData->TryGetNumberField(TEXT("Location.X"), PointLoc.X);
			SinglePointData->TryGetNumberField(TEXT("Location.Y"), PointLoc.Y);
			SinglePointData->TryGetNumberField(TEXT("Location.Z"), PointLoc.Z);
			FVector PointTan;
			SinglePointData->TryGetNumberField(TEXT("Tangent.X"), PointTan.X);
			SinglePointData->TryGetNumberField(TEXT("Tangent.Y"), PointTan.Y);
			SinglePointData->TryGetNumberField(TEXT("Tangent.Z"), PointTan.Z);
			FRotator PointRot;
			SinglePointData->TryGetNumberField(TEXT("Rotation.Yaw"), PointRot.Yaw);
			SinglePointData->TryGetNumberField(TEXT("Rotation.Pitch"), PointRot.Pitch);
			SinglePointData->TryGetNumberField(TEXT("Rotation.Roll"), PointRot.Roll);
			//处理Owner缩放对Spline的影响
			//PointLoc = UKismetMathLibrary::TransformLocation(OwnerTransform, PointLoc);
			PointsType.Emplace(PointType);
			PointsLoc.Emplace(PointLoc);
			PointsTan.Emplace(PointTan);
			PointsRot.Emplace(PointRot);
		}
		bool bIsCloseLoop = false;
		SingleSplineData->TryGetBoolField(TEXT("bCloseLoop"), bIsCloseLoop);
		//@TODO：后续看量改成分帧处理
		if (!SpawnedActors.Contains(OwnerActorGuid))
		{
			TObjectPtr<AActor> SplineActor = UEditorComponentUtilities::SpawnEmptyActor(OwnerActorName, OwnerTransform);
			//SplineActor->SetActorTransform(OwnerTransform);
			if (bTryParseActorTag && !ActorTags.IsEmpty())
			{
				SplineActor->Tags = ActorTags;
			}
			SpawnedActors.Emplace(OwnerActorGuid, SplineActor);
		}
		TObjectPtr<USplineComponent> NewSplineComp = AddSplineCompToExistActor(
			SpawnedActors[OwnerActorGuid], PointsType, PointsLoc, PointsTan, PointsRot,
			bIsCloseLoop);
		if (NewSplineComp != nullptr && bTryParseCompTag && !SplineCompTags.IsEmpty())
		{
			NewSplineComp->ComponentTags = SplineCompTags;
		}
	};
	if (bAutoCollectAfterSpawn)
	{
		CollectAllSplines();
	}
}

/*TObjectPtr<AActor> UCityGeneratorSubSystem::SpawnEmptyActor(const FString& ActorName, const FTransform& ActorTrans)
{
	FActorSpawnParameters SpawnParams;
	//SpawnParams.Name=FName(*ActorName);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TObjectPtr<AActor> NewActor = GetEditorContext()->SpawnActor<AActor>(
		AActor::StaticClass(), ActorTrans, SpawnParams);
	TObjectPtr<UActorComponent> SceneComp = AddComponentInEditor(NewActor, USceneComponent::StaticClass());
	TObjectPtr<USceneComponent> RootComp = Cast<USceneComponent>(SceneComp);
	NewActor->SetRootComponent(RootComp);
	NewActor->SetActorLabel(ActorName);
	NewActor->SetActorTransform(ActorTrans);
	return NewActor;
}*/

TObjectPtr<USplineComponent> UCityGeneratorSubSystem::AddSplineCompToExistActor(
	TObjectPtr<AActor> TargetActor, const TArray<int32>& PointsType,
	const TArray<FVector>& PointsLoc,
	const TArray<FVector>& PointsTangent,
	const TArray<FRotator>& PointsRotator, const bool bIsCloseLoop)
{
	//有效性检查
	if (nullptr == TargetActor)
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Null Actor Passed In");
		return nullptr;
	}
	if (PointsLoc.IsEmpty())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Found Null Data");
		return nullptr;
	}
	if (PointsLoc.Num() != PointsTangent.Num() || PointsTangent.Num() != PointsRotator.Num())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Non-Homogeneous Data");
		return nullptr;
	}


	USplineComponent* SplineComp = Cast<USplineComponent>(
		UEditorComponentUtilities::AddComponentInEditor(TargetActor, USplineComponent::StaticClass()));

	SplineComp->SetSplinePoints(PointsLoc, ESplineCoordinateSpace::Local, false);
	for (int i = 0; i < PointsLoc.Num(); ++i)
	{
		SplineComp->SetTangentAtSplinePoint(i, PointsTangent[i], ESplineCoordinateSpace::Local, false);
		SplineComp->SetRotationAtSplinePoint(i, PointsRotator[i], ESplineCoordinateSpace::Local, false);
		//设置Tangent会覆盖Type，必须放在后边
		ESplinePointType::Type PointType = ESplinePointType::Type(PointsType[i]);
		SplineComp->SetSplinePointType(i, PointType, true);
	}
	SplineComp->SetClosedLoop(bIsCloseLoop);
	SplineComp->UpdateSpline();
	return SplineComp;
}

/*TObjectPtr<UActorComponent> UCityGeneratorSubSystem::AddComponentInEditor(AActor* TargetActor,
                                                                          TSubclassOf<UActorComponent>
                                                                          TargetComponentClass)
{
	//有效性检查
	if (nullptr == TargetActor)
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Null Actor Passed In");
		return nullptr;
	}
	//实现方法1
	/*GEditor->BeginTransaction(TEXT("AddComponent"), FText(), nullptr);
	UKismetSystemLibrary::TransactObject(TargetActor);
	USubobjectDataSubsystem* AddCompSubSystem = GEngine->GetEngineSubsystem<USubobjectDataSubsystem>();
	TArray<FSubobjectDataHandle> SubObjectData;
	AddCompSubSystem->GatherSubobjectData(TargetActor, SubObjectData);
	FAddNewSubobjectParams NewCompParam;
	NewCompParam.ParentHandle = SubObjectData[0];
	NewCompParam.NewClass = TargetComponentClass;
	FText FailReason;
	FSubobjectDataHandle AddHandled = AddCompSubSystem->AddNewSubobject(NewCompParam, FailReason);
	if (!AddHandled.IsValid())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner(FailReason.ToString());
		return nullptr;
	}
	const UActorComponent* ConstNewComp = Cast<UActorComponent>(AddHandled.GetData()->GetObject());
	GEditor->EndTransaction();
	return const_cast<UActorComponent*>(ConstNewComp);#1#
	//实现方法2（https://forums.unrealengine.com/t/add-component-to-actor-in-c-the-final-word/646838/9）
	TargetActor->Modify();
	TObjectPtr<UActorComponent> NewComponent = NewObject<UActorComponent>(TargetActor, TargetComponentClass);
	NewComponent->OnComponentCreated();
	TObjectPtr<USceneComponent> NewComponentAsSceneComp = Cast<USceneComponent>(NewComponent);
	if (nullptr != NewComponentAsSceneComp)
	{
		NewComponentAsSceneComp->AttachToComponent(TargetActor->GetRootComponent(),
		                                           FAttachmentTransformRules::SnapToTargetIncludingScale);
	}
	NewComponent->RegisterComponent();
	TargetActor->AddInstanceComponent(NewComponent);
	return NewComponent;
}*/


TObjectPtr<UWorld> UCityGeneratorSubSystem::GetEditorContext() const
{
	UUnrealEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();
	ensureMsgf(EditorSubsystem!=nullptr, TEXT("Get EditorSubsystem Failed,Please Check"));
	return EditorSubsystem->GetEditorWorld();
}


#pragma region GenerateRoad
void UCityGeneratorSubSystem::GenerateRoads(USplineComponent* TargetSpline)
{
	if (nullptr == TargetSpline)
	{
		EAppReturnType::Type UserChoice = UNotifyUtilities::ShowMsgDialog(EAppMsgType::YesNo,
		                                                                  "Select None SplineComp,Click [Yes] To Generate All,Click [No] To Quit",
		                                                                  true);
		if (UserChoice == EAppReturnType::Yes)
		{
			CollectAllSplines();
			for (const TWeakObjectPtr<USplineComponent>& SplineComponent : CityGeneratorSplineArray)
			{
				if (SplineComponent.IsValid())
				{
					GetRoadGeneratorSystem().Pin()->GenerateSingleRoadBySweep(SplineComponent.Pin().Get());
				}
			}
		}

		return;
	}
	GetRoadGeneratorSystem().Pin()->GenerateSingleRoadBySweep(TargetSpline);
	return;
}

TWeakObjectPtr<URoadGeneratorSubsystem> UCityGeneratorSubSystem::GetRoadGeneratorSystem()
{
	if (!RoadSubsystem.IsValid())
	{
		RoadSubsystem = GEditor->GetEditorSubsystem<URoadGeneratorSubsystem>();
		ensureAlwaysMsgf(RoadSubsystem.IsValid(), TEXT("Find Null RoadGeneratorSubsystem"));
	}
	return RoadSubsystem;
}


#pragma endregion  Base
/*void UCityGeneratorSubSystem::GenerateSingleRoadBySweep(const USplineComponent* TargetSpline,
                                                        const TArray<FVector2D>& SweepShape)
{
	if (nullptr == TargetSpline || nullptr == TargetSpline->GetOwner())
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Fail To Create SweepMesh Invalid Spline");
		return;
	}
	FString SplineOwnerName = TargetSpline->GetOwner()->GetActorLabel() + "_GenMesh";
	FTransform SplineOwnerTransform = TargetSpline->GetOwner()->GetTransform();
	SplineOwnerTransform.SetScale3D(FVector(1.0, 1.0, 1.0));
	TObjectPtr<AActor> MeshActor = SpawnEmptyActor(SplineOwnerName, SplineOwnerTransform);
	TObjectPtr<UDynamicMeshComponent> DynamicMeshComp = Cast<UDynamicMeshComponent>(
		AddComponentInEditor(MeshActor, UDynamicMeshComponent::StaticClass()));
	if (nullptr == DynamicMeshComp)
	{
		UNotifyUtilities::ShowPopupMsgAtCorner("Generate Mesh Failed");
		return;
	}
	UDynamicMesh* DynamicMesh = DynamicMeshComp->GetDynamicMesh();
	DynamicMesh->Reset();
	FGeometryScriptPrimitiveOptions GeometryScriptOptions;
	FTransform SweepMeshTrans = FTransform::Identity;
	int32 ControlPointsCount = TargetSpline->GetNumberOfSplinePoints();
	TArray<FTransform> SweepPath = ResampleSamplePoint(TargetSpline);
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(DynamicMesh, GeometryScriptOptions,
	                                                                  SweepMeshTrans, SweepShape, SweepPath);
	UGeometryScriptLibrary_MeshNormalsFunctions::AutoRepairNormals(DynamicMesh);
	FGeometryScriptSplitNormalsOptions SplitOptions;
	FGeometryScriptCalculateNormalsOptions CalculateOptions;
	UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(DynamicMesh, SplitOptions, CalculateOptions);
}

TArray<FTransform> UCityGeneratorSubSystem::ResampleSamplePoint(const USplineComponent* TargetSpline,
                                                                double StartShrink, double EndShrink)
{
	TArray<FTransform> ResamplePointsOnSpline{FTransform::Identity};
	if (nullptr != TargetSpline)
	{
		//直线和曲线有不同的差值策略
		//点属性管理的是后边一段
		const int32 OriginControlPoints=TargetSpline->GetNumberOfSplinePoints();
		for (int i = 0; i < OriginControlPoints; ++i)
		{
			ESplinePointType::Type PointType=TargetSpline->GetSplinePointType()
		}
		
		double ResampleSplineLength = TargetSpline->GetSplineLength() - StartShrink - EndShrink;
		int32 ResamplePointCount = FMath::CeilToInt(ResampleSplineLength / CurveResampleLengthInCM);
		//double ActualResampleLengthInCM = ResampleSplineLength / ResamplePointCount;
		ResamplePointsOnSpline.SetNum(TargetSpline->IsClosedLoop() ? ResamplePointCount + 1 : ResamplePointCount);
		for (int32 i = 0; i < ResamplePointCount; ++i)
		{
			double DistanceFromSplineStart = FMath::Clamp(i * CurveResampleLengthInCM+ StartShrink,StartShrink,ResampleSplineLength+StartShrink);
			ResamplePointsOnSpline[i] = TargetSpline->GetTransformAtDistanceAlongSpline(
				DistanceFromSplineStart, ESplineCoordinateSpace::Local, true);
		}
		if (TargetSpline->IsClosedLoop())
		{
			FTransform ResampleStartPointTrans = TargetSpline->GetTransformAtDistanceAlongSpline(
				StartShrink, ESplineCoordinateSpace::Local, true);
			ResamplePointsOnSpline[ResamplePointCount]=ResampleStartPointTrans;
		}
		//@TODO:ActorTransform会造成影响
		/*if (nullptr!=TargetSpline->GetOwner())
		{
			const FTransform ActorTrans=TargetSpline->GetOwner()->GetActorTransform();
			for (auto& PointsOnSpline : ResamplePointsOnSpline)
			{
				PointsOnSpline.SetLocation(UKismetMathLibrary::TransformLocation(ActorTrans, PointsOnSpline.GetLocation()));
			}
		}#1#
	}
	return MoveTemp(ResamplePointsOnSpline);
}*/
#pragma endregion GenerateRoad
