// Fill out your copyright notice in the Description page of Project Settings.


#include "EditorComponentUtilities.h"

#include "NotifyUtilities.h"
#include "Subsystems/UnrealEditorSubsystem.h"

TObjectPtr<AActor> UEditorComponentUtilities::SpawnEmptyActor(const FString& ActorName, const FTransform& ActorTrans)
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
}

TObjectPtr<UActorComponent> UEditorComponentUtilities::AddComponentInEditor(AActor* TargetActor,
	TSubclassOf<UActorComponent> TargetComponentClass)
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
	return const_cast<UActorComponent*>(ConstNewComp);*/
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
}

TObjectPtr<UWorld> UEditorComponentUtilities::GetEditorContext()
{
	UUnrealEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();
	ensureMsgf(EditorSubsystem!=nullptr, TEXT("Get EditorSubsystem Failed,Please Check"));
	return EditorSubsystem->GetEditorWorld();
}
