// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NotifyUtilities.generated.h"

/**
 * 
 */
UCLASS()
class JIAPCGAIDTOOL_API UNotifyUtilities : public UObject
{
	GENERATED_BODY()
public:
	static EAppReturnType::Type ShowMsgDialog(EAppMsgType::Type MsgType,const FString& DisplayMessage,bool bShowMsgAsWarning=false);
	static void ShowPopupMsgAtCorner(const FString& Message);
};
