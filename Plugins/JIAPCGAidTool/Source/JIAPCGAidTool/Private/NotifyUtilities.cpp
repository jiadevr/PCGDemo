// Fill out your copyright notice in the Description page of Project Settings.


#include "NotifyUtilities.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
DEFINE_LOG_CATEGORY(JIAPCGAidTool)

EAppReturnType::Type UNotifyUtilities::ShowMsgDialog(EAppMsgType::Type MsgType, const FString& DisplayMessage,
                                                     bool bShowMsgAsWarning)
{
	FText TitleText = bShowMsgAsWarning
		                  ? FText::FromStringView(TEXT("Warning"))
		                  : FText::FromStringView(TEXT("Message"));
	return FMessageDialog::Open(MsgType, FText::FromStringView(DisplayMessage), TitleText);
}

void UNotifyUtilities::ShowPopupMsgAtCorner(const FString& Message)
{
	//主标题设置
	FNotificationInfo NotificationInfo(INVTEXT("JiaPCGAidTool"));
	NotificationInfo.bUseLargeFont = true;
	//文本内容设置
	NotificationInfo.SubText = FText::FromStringView(Message);
	//显示时间
	NotificationInfo.ExpireDuration = 10.0f;
	NotificationInfo.FadeInDuration = 0.5f;
	//显示图标
	NotificationInfo.bUseSuccessFailIcons = false;
	/*//可选添加按钮,注意最后一个参数必须传入，如果使用SNotificationItem::CS_Pending按钮不会显示
	NotificationInfo.ButtonDetails.Add(
		FNotificationButtonInfo(INVTEXT("ButtonText"),INVTEXT("ButtonTip"), FSimpleDelegate(),
		                        SNotificationItem::CS_None));*/
	FSlateNotificationManager::Get().AddNotification(NotificationInfo);
	UE_LOG(JIAPCGAidTool, Display, TEXT("%s"), *Message);
}
