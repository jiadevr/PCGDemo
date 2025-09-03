// Fill out your copyright notice in the Description page of Project Settings.


#include "CityGeneratorSubSystem.h"

#include "NotifyUtilities.h"
#include "Components/SplineComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystems/UnrealEditorSubsystem.h"

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
	SceneSplinesData->SetStringField("CoordSpace","Local");
	//场景中所有Spline数据
	TArray<TSharedPtr<FJsonValue>> SplineDataArray;
	int32 SplineCounter=0;
	for (const TWeakObjectPtr<USplineComponent> SingleSpline : CityGeneratorSplineArray)
	{
		if (!SingleSpline.IsValid()) { continue; }
		TStrongObjectPtr<USplineComponent> TargetSpline = SingleSpline.Pin();
		AActor* SplineOwner=TargetSpline->GetOwner();

		//单个Spline数据
		TSharedPtr<FJsonObject> SingleSplineData(new FJsonObject());
		//SplineOwner信息输入
		if (nullptr==SplineOwner){continue;}
		SingleSplineData->SetNumberField("Index",SplineCounter);
		//ActorTag和CompTag
		if (bSaveActorTag&&!SplineOwner->Tags.IsEmpty())
		{
			TArray<TSharedPtr<FJsonValue>> ActorTags;
			int32 ActorTagCounter=0;
			for (const FName& ActorTag : SplineOwner->Tags)
			{
				TSharedPtr<FJsonObject> SingleTag;
				FString TagName="Tag"+ActorTagCounter;
				SingleTag->SetStringField(TagName,ActorTag.ToString());
				ActorTags.Emplace(MakeShareable(new FJsonValueObject(SingleTag)));
				ActorTagCounter++;
			}
			SingleSplineData->SetArrayField("ActorTags",ActorTags);
		}
		if (bSaveCompTag&&!TargetSpline->ComponentTags.IsEmpty())
		{
			TArray<TSharedPtr<FJsonValue>> ComponentTags;
			int32 CompTagCounter=0;
			for (const FName& CompTag : TargetSpline->ComponentTags)
			{
				TSharedPtr<FJsonObject> SingleTag;
				FString TagName="Tag"+CompTagCounter;
				SingleTag->SetStringField(TagName,CompTag.ToString());
				ComponentTags.Emplace(MakeShareable(new FJsonValueObject(SingleTag)));
				CompTagCounter++;
			}
			SingleSplineData->SetArrayField("CompTags",ComponentTags);
		}
		//ActorTransform
		FTransform OwnerTransform=SplineOwner->GetActorTransform();
		SingleSplineData->SetNumberField("Location.X",OwnerTransform.GetLocation().X);
		SingleSplineData->SetNumberField("Location.Y",OwnerTransform.GetLocation().Y);
		SingleSplineData->SetNumberField("Location.Z",OwnerTransform.GetLocation().Z);
		
		SingleSplineData->SetNumberField("Rotation.X",OwnerTransform.GetRotation().X);
		SingleSplineData->SetNumberField("Rotation.Y",OwnerTransform.GetRotation().Y);
		SingleSplineData->SetNumberField("Rotation.Z",OwnerTransform.GetRotation().Z);

		SingleSplineData->SetNumberField("Transform.X",OwnerTransform.GetScale3D().X);
		SingleSplineData->SetNumberField("Transform.Y",OwnerTransform.GetScale3D().Y);
		SingleSplineData->SetNumberField("Transform.Z",OwnerTransform.GetScale3D().Z);
		//SplineControlPoint信息
		TArray<TSharedPtr<FJsonValue>> SingleSplinePointsArray;
		int32 ControlPointCount=TargetSpline->GetNumberOfSplinePoints();
		TArray<FVector> PointsLoc;
		PointsLoc.SetNum(ControlPointCount);
		TArray<FRotator> PointsRot;
		PointsRot.SetNum(ControlPointCount);
		TArray<FVector> PointsTan;
		PointsTan.SetNum(ControlPointCount);
		
		for (int i = 0; i <ControlPointCount  ; ++i)
		{
			TargetSpline->GetLocationAndTangentAtSplinePoint(i,PointsLoc[i],PointsTan[i],ESplineCoordinateSpace::Local);
			PointsRot[i]=TargetSpline->GetRotationAtSplinePoint(i,ESplineCoordinateSpace::Local);
		}
		for (int i = 0; i < PointsLoc.Num(); ++i)
		{
			TSharedPtr<FJsonObject> SinglePointData(new FJsonObject());
			SinglePointData->SetNumberField("Location.X",PointsLoc[i].X);
			SinglePointData->SetNumberField("Location.Y",PointsLoc[i].Y);
			SinglePointData->SetNumberField("Location.Z",PointsLoc[i].Z);
			
			SinglePointData->SetNumberField("Tangent.X",PointsLoc[i].X);
			SinglePointData->SetNumberField("Tangent.Y",PointsLoc[i].Y);
			SinglePointData->SetNumberField("Tangent.Z",PointsLoc[i].Z);
			
			SinglePointData->SetNumberField("Rotation.X",PointsLoc[i].X);
			SinglePointData->SetNumberField("Rotation.Y",PointsLoc[i].Y);
			SinglePointData->SetNumberField("Rotation.Z",PointsLoc[i].Z);
			SingleSplinePointsArray.Emplace(MakeShareable(new FJsonValueObject(SinglePointData)));
		}
		SingleSplineData->SetArrayField("Points",SingleSplinePointsArray);
		SplineDataArray.Emplace(MakeShareable(new FJsonValueObject(SingleSplineData)));
		SplineCounter++;
	}
	SceneSplinesData->SetArrayField("Splines",SplineDataArray);
	FString SerializeStr;
	TSharedRef<TJsonWriter<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter=TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&SerializeStr);

	FJsonSerializer::Serialize(SceneSplinesData.ToSharedRef(),JsonWriter);
	UE_LOG(LogTemp,Display,TEXT("Json Res:[%s]"),*SerializeStr);
	FFileHelper::SaveStringToFile(SerializeStr,*TargetDir);
	UNotifyUtilities::ShowPopupMsgAtCorner("Save Scene Spline Data Finished!");
}


void UCityGeneratorSubSystem::GenerateSingleRoadBySweep(const USplineComponent* TargetSpline,
                                                        const TArray<FVector2D>& SweepShape)
{
	FVector SplineLocation = TargetSpline->GetLocationAtSplineInputKey(0, ESplineCoordinateSpace::World);
}

TObjectPtr<UObject> UCityGeneratorSubSystem::GetEditorContext() const
{
	UUnrealEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();
	ensureMsgf(EditorSubsystem!=nullptr, TEXT("Get EditorSubsystem Failed,Please Check"));
	return EditorSubsystem->GetEditorWorld();
}
