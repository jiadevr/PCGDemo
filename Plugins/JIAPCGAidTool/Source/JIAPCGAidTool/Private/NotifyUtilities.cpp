// Fill out your copyright notice in the Description page of Project Settings.


#include "NotifyUtilities.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

EAppReturnType::Type UNotifyUtilities::ShowMsgDialog(EAppMsgType::Type MsgType, const FString& DisplayMessage,
                                                      bool bShowMsgAsWarning)
{
	FText TitleText=bShowMsgAsWarning?FText::FromStringView(TEXT("Warning")):FText::FromStringView(TEXT("Message"));
	return FMessageDialog::Open(MsgType,FText::FromStringView(DisplayMessage),TitleText);
}

void UNotifyUtilities::ShowPopupMsgAtCorner(const FString& Message)
{
	FNotificationInfo NotificationInfo(FText::FromStringView(Message));
	NotificationInfo.bUseLargeFont=true;
	NotificationInfo.FadeInDuration=10.0f;
	FSlateNotificationManager::Get().AddNotification(NotificationInfo);
}
