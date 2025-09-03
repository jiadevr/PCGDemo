// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Logging/LogMacros.h"
#include "NotifyUtilities.generated.h"

/**
 * 
 */
DECLARE_LOG_CATEGORY_EXTERN(JIAPCGAidTool,Log,All);
UCLASS()
class JIAPCGAIDTOOL_API UNotifyUtilities : public UObject
{
	GENERATED_BODY()
public:
	static EAppReturnType::Type ShowMsgDialog(EAppMsgType::Type MsgType,const FString& DisplayMessage,bool bShowMsgAsWarning=false);
	static void ShowPopupMsgAtCorner(const FString& Message);
};
